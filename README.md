# Projeto IT — Engenharia de Dados aplicada ao setor hospitalar

Projeto prático de Engenharia de Dados voltado ao rastreamento de deslocamento de pacientes em ambientes hospitalares utilizando sensores BLE. Esta branch (`local`) contém a **stack de desenvolvimento local, totalmente containerizada**, executando um pipeline completo de ingestão, transformação e visualização sem dependência de serviços de nuvem pagos.

> A arquitetura original em nuvem (Google Cloud Platform) está preservada na branch [`main`](../../tree/main), mantida como referência da evolução do projeto.

O objetivo é capturar eventos de proximidade emitidos por beacons BLE, estruturar um pipeline moderno em camadas e transformar os dados em informações sobre fluxo, permanência e deslocamento entre ambientes.

---

# Problema de negócio

Hospitais possuem fluxos complexos de deslocamento:

- pacientes buscando setores
- filas internas
- pontos de congestionamento
- dificuldade de wayfinding (orientação interna)
- baixa visibilidade operacional

Sem dados estruturados, oportunidades de melhoria ficam invisíveis.

---

# Solução

Dispositivos ESP32 atuam como gateways BLE, captando sinais de beacons associados a perfis de paciente (idoso, jovem e mobilidade reduzida) posicionados em três ambientes de referência (Recepção, Sala de Espera e Consultório 1). Os eventos são enviados em lote para uma API de ingestão, persistidos em PostgreSQL, transformados em camadas com dbt e expostos em um painel HTML autocontido e no Metabase.

---

# Arquitetura local

```text
ESP32 (3 gateways) + Beacons BLE (3 perfis)
→ FastAPI (API de ingestão, /ingest e /ingest/batch)
→ PostgreSQL 16 (schema it_raw)
→ dbt-postgres (staging → intermediate → marts)
→ Painel HTML autocontido + Metabase
```

Toda a stack sobe via Docker Compose (contêineres `postgres`, `api` e `metabase`).

---

# Stack utilizada

| Camada | Tecnologia |
|---|---|
| Edge / IoT | ESP32 + BLE |
| API de Ingestão | Python + FastAPI |
| Containerização | Docker + Docker Compose |
| Banco de dados | PostgreSQL 16 |
| Transformação | dbt Core + dbt-postgres |
| Visualização | Painel HTML (Chart.js + SVG) + Metabase |
| Versionamento | Git + GitHub |

---

# Camadas de dados

O pipeline segue separação estrita de responsabilidades:

- **it_raw** — preserva integralmente todos os eventos recebidos dos gateways
- **it_staging** — sinaliza problemas de qualidade (sem remover dados)
- **it_intermediate** — enriquece os eventos (join com beacons e gateways) sem filtrar
- **it_marts** — aplica as regras de negócio (localização atual, tempo por local, ausências, trajeto)

---

# Dataset de demonstração

O diretório [`data/`](./data) contém um dump dos eventos reais capturados em bancada (3 ESP32 + 3 beacons BLE, coletados em ambiente residencial controlado entre 01/06 e 25/06/2026), permitindo reproduzir o pipeline completo sem hardware.

- Arquivo: `data/esp32_payload.csv` (66.772 eventos)
- Os dados provêm de um ambiente acadêmico de bancada, sem qualquer informação de pacientes reais — os "perfis" (idoso, jovem, mobilidade reduzida) são apenas rótulos associados a beacons de teste.
- Os endereços MAC correspondem ao seed `dbt/seeds/beacons.csv`, garantindo que os joins do pipeline funcionem diretamente.

---

# Como executar

```bash
# 1. Configurar variáveis de ambiente
cp .env.example .env    # ajuste API_KEY e senha do Postgres

# 2. Subir a stack
docker compose -f infra/docker-compose.yml up -d --build

# 3. Criar o schema e a tabela raw
docker exec -i projeto_it_postgres psql -U projeto_it -d projeto_it < infra/sql/001_create_raw_schema.sql

# 4. Carregar o dataset de demonstração
docker exec -i projeto_it_postgres psql -U projeto_it -d projeto_it \
  -c "\copy it_raw.esp32_payload(id,ingested_at,gateway_id,device_timestamp,tag_mac,found,rssi) FROM 'data/esp32_payload.csv' WITH (FORMAT csv, HEADER true)"

# 5. Rodar o dbt
dbt seed --profiles-dir .
dbt run  --profiles-dir .

# 6. Abrir o painel
open dashboard_it_guilherme.html   # consome a API em http://localhost:8080
```

---

# Caso real resolvido (RCA)

Durante a fase de nuvem, a integração Pub/Sub → BigQuery apresentou erro `invalid_argument`. Após uma análise de causa raiz (Root Cause Analysis), o contrato do evento foi refatorado de payload aninhado para schema flat, restabelecendo a ingestão. Já na migração para a stack local, um segundo troubleshooting relevante envolveu a correção de macros de nomeação de schema no dbt-postgres (evitando duplicação de schemas) e o ajuste de endpoints de trajeto para ancorar consultas na data mais recente dos dados, em vez da data corrente.

---

# Diferenciais do projeto

- Aplicação em problema real do setor hospitalar
- Pipeline de dados completo, do sensor ao dashboard
- Duas arquiteturas versionadas (nuvem e local) para comparação
- Integração entre IoT e Engenharia de Dados
- Dataset real de bancada para reprodutibilidade
- Troubleshooting real de pipeline documentado

---

# Sobre o autor

Guilherme Brevilato é profissional com experiência prévia no setor hospitalar e trajetória em engenharia, atualmente direcionando carreira para Engenharia de Dados. Este projeto representa a união entre conhecimento operacional de saúde e tecnologia de dados aplicada a problemas reais.

---

# Contato

LinkedIn: https://linkedin.com/in/guilhermebrevilato  
GitHub: https://github.com/GuilhermeBrevilato
