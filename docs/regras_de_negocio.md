# Regras de Negócio — Modelos dbt

**Projeto IT — Rastreamento de Pacientes com BLE**
Universidade Presbiteriana Mackenzie · 2026

---

## Contexto

O pipeline de dados do Projeto IT transforma eventos BLE brutos coletados pelos gateways ESP32 em informações clínicas sobre localização e movimentação de pacientes. As regras de negócio descritas neste documento estão implementadas nos modelos dbt da camada `intermediate` e `marts`.

---

## Camadas do pipeline

```
it_raw.esp32_payload          ← evento bruto do ESP32
    ↓
stg_ble_events (view)         ← tipagem, renomeação e campos derivados
    ↓
int_ble_events (view)         ← enriquecimento com grupo clínico e local físico
    ↓
mart_localizacao_atual        ← onde está cada grupo agora
mart_tempo_por_local          ← quanto tempo cada grupo passou em cada local
mart_trajeto_do_dia           ← sequência de locais visitados no dia
mart_localizacao_por_hora     ← localização predominante por hora
mart_ausencia_beacon          ← períodos sem detecção por nenhum gateway
```

---

## Seeds de referência

### beacons.csv

Mapeia cada beacon BLE para um grupo clínico.

| Campo | Descrição |
|---|---|
| `tag_mac` | Endereço MAC do beacon no formato `aa:bb:cc:dd:ee:ff` |
| `grupo` | Grupo clínico: `idoso`, `mobilidade_reduzida` ou `jovem` |
| `descricao` | Descrição do grupo |

**Grupos definidos:**

| tag_mac | grupo | descricao |
|---|---|---|
| `7c:ec:79:47:73:62` | idoso | Pacientes com 60 anos ou mais |
| `d4:f5:13:79:e4:5b` | mobilidade_reduzida | Pacientes cadeirantes ou com dificuldade de locomoção |
| `7c:ec:79:44:e0:5f` | jovem | Pacientes com menos de 60 anos sem restrição de mobilidade |

### gateways.csv

Mapeia cada gateway ESP32 para um local físico.

| Campo | Descrição |
|---|---|
| `gateway_id` | Identificador do gateway ESP32 |
| `local` | Nome do local onde o gateway está instalado |
| `andar` | Andar do local |
| `descricao` | Descrição detalhada do local |

**Locais definidos:**

| gateway_id | local | andar |
|---|---|---|
| `gw-esp32-01` | Recepção | Térreo |
| `gw-esp32-02` | Sala de Espera | Térreo |
| `gw-esp32-03` | Consultório 1 | Térreo |

---

## Regra transversal — Gateway dominante

Quando o mesmo beacon é detectado por múltiplos gateways simultaneamente, o local do paciente é determinado pelo **gateway com maior RSSI** — ou seja, o mais próximo fisicamente.

```
RSSI mais próximo de zero = sinal mais forte = gateway mais próximo = local do paciente
```

**Exemplo:**

| gateway_id | local | rssi |
|---|---|---|
| gw-esp32-01 | Recepção | -72 |
| gw-esp32-02 | Sala de Espera | -85 |
| gw-esp32-03 | Consultório 1 | -91 |

→ Paciente localizado na **Recepção** (RSSI -72, maior entre os três).

Esta regra é aplicada nos modelos `mart_trajeto_do_dia` e `mart_localizacao_por_hora` e será central no intermediate evoluído.

---

## Regras de qualidade de evento

Aplicadas no `stg_ble_events` e herdadas por todos os modelos downstream:

### is_valid_rssi

```sql
case
    when rssi between -120 and 0 then true
    else false
end as is_valid_rssi
```

Valida se o RSSI está dentro da faixa física possível para BLE. Valores fora desse intervalo — como `-127` gerado quando o beacon não é detectado — indicam ausência de sinal, não um sinal fraco.

### is_core_event_valid

```sql
case
    when gateway_id is not null
     and device_timestamp is not null
     and tag_mac is not null
    then true
    else false
end as is_core_event_valid
```

Valida se os três campos obrigatórios para identificar um evento estão presentes. Eventos sem esses campos não podem ser associados a um paciente ou local.

**Todos os marts aplicam o filtro:**
```sql
where found = true
  and is_valid_rssi = true
  and is_core_event_valid = true
```

---

## mart_localizacao_atual

**Pergunta:** Onde está cada grupo agora?

**Lógica:**

1. Filtra eventos válidos — `found = true`, `is_valid_rssi = true`, `is_core_event_valid = true`
2. Para cada grupo, ordena por `event_timestamp_utc DESC` usando `ROW_NUMBER()`
3. Seleciona apenas o registro mais recente de cada grupo (`rn = 1`)
4. Descarta registros com `grupo IS NULL` ou `local IS NULL` — beacons e gateways não cadastrados nos seeds
5. Calcula `minutos_desde_ultimo_sinal` como `TIMESTAMP_DIFF(current_timestamp(), event_timestamp_utc, MINUTE)`

**Resultado:** uma linha por grupo clínico com o local, RSSI e timestamp do último sinal detectado.

**Limitação conhecida:** não distingue se o paciente está no local agora ou se foi o último local antes de sair do alcance. O campo `minutos_desde_ultimo_sinal` permite ao analista identificar registros desatualizados.

---

## mart_tempo_por_local

**Pergunta:** Quanto tempo cada grupo passou em cada local?

**Lógica:**

1. Filtra eventos válidos
2. Agrupa por `event_date + grupo + local + andar + gateway_id`
3. Calcula tempo de permanência:

```sql
TIMESTAMP_DIFF(
    MAX(event_timestamp_utc),
    MIN(event_timestamp_utc),
    MINUTE
) as minutos_no_local
```

4. Calcula RSSI médio, mínimo e máximo por combinação
5. Conta total de registros como medida de densidade de cobertura

**Limitação conhecida:** se o paciente sair e voltar ao mesmo local no mesmo dia, o modelo trata como uma permanência contínua — não detecta a saída intermediária. A correção será implementada com lógica de sessão por local no intermediate.

---

## mart_trajeto_do_dia

**Pergunta:** Qual foi a sequência de locais visitados?

**Lógica:**

1. Filtra eventos válidos
2. Aplica a regra do gateway dominante — para cada timestamp, seleciona o gateway com maior RSSI:

```sql
row_number() over (
    partition by grupo, event_timestamp_utc
    order by rssi desc
) as rn_rssi
```

3. Detecta mudanças de local usando `LAG(local)`:

```sql
lag(local) over (
    partition by grupo, event_date
    order by event_timestamp_utc
) as local_anterior
```

4. Marca início de nova visita quando `local != local_anterior`
5. Cria `sessao_id` acumulando as marcações com `SUM() OVER`
6. Agrega cada sessão contínua: chegada, saída, duração em minutos e RSSI médio

**Resultado:** sequência ordenada de visitas por grupo por dia, com horário de chegada e saída em cada local.

---

## mart_localizacao_por_hora

**Pergunta:** Em qual local cada grupo estava a cada hora do dia?

**Lógica:**

1. Filtra eventos válidos
2. Agrupa por `event_date + event_hour + grupo + local`
3. Para cada hora, seleciona o local com maior RSSI médio usando `ROW_NUMBER()`:

```sql
row_number() over (
    partition by event_date, event_hour, grupo
    order by rssi_medio desc
) as rn
```

4. O local com sinal mais forte na hora é o local predominante naquele período

**Resultado:** uma linha por combinação de grupo + hora + dia, mostrando onde cada grupo estava predominantemente a cada hora.

---

## mart_ausencia_beacon

**Pergunta:** Algum beacon ficou sem detecção por mais de 10 minutos?

**Lógica:**

1. Para cada momento, verifica se o beacon foi detectado por **qualquer** gateway:

```sql
MAX(case when found = true then 1 else 0 end) as detectado
```

2. Usa `LAG(detectado)` para identificar quando o beacon voltou a ser detectado após período de ausência
3. Calcula o gap entre o retorno e a última detecção:

```sql
TIMESTAMP_DIFF(event_timestamp_utc, timestamp_anterior, MINUTE) as minutos_ausente
```

4. Filtra apenas gaps **≥ 10 minutos**

**Threshold de 10 minutos:** definido com base no ciclo de coleta de 15 minutos. Qualquer ausência menor pode ser ruído de scan — um beacon fora do alcance por menos de 10 minutos pode simplesmente não ter sido detectado num ciclo.

**Resultado:** períodos de ausência confirmada, com horário de início, fim e duração. Zero linhas indica cobertura contínua de todos os beacons.

---

## Limitações atuais e evoluções planejadas

| Limitação | Impacto | Evolução planejada |
|---|---|---|
| `mart_tempo_por_local` trata o dia inteiro como uma permanência | Superestima tempo em locais visitados mais de uma vez | Implementar lógica de sessão por local no intermediate |
| Localização baseada apenas em RSSI | Imprecisão em ambientes com muita interferência de RF | Calibrar com medições reais por local e aplicar filtro de média móvel |
| Threshold de ausência fixo em 10 min | Pode não capturar ausências curtas relevantes | Parametrizar threshold por grupo clínico |
| Sem distinção entre em consulta e em espera | Não identifica se o paciente está sendo atendido | Definir threshold de permanência por local — beacon parado por X min num único gateway indica atendimento |

---

## Resultados validados em teste

| Métrica | Valor |
|---|---|
| Total de registros no BigQuery | 15.400+ |
| Gateways ativos | 3 (gw-esp32-01, gw-esp32-02, gw-esp32-03) |
| Grupos clínicos monitorados | 3 (idoso, mobilidade_reduzida, jovem) |
| Modelos dbt rodando | 7 (PASS=7 WARN=0 ERROR=0) |
| Ausências detectadas no teste | 0 — cobertura contínua confirmada |
| RSSI médio geral | -61.8 dBm |
| Taxa de detecção (found=true) | 98.4% |
