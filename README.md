# projeto-it

Sistema de rastreamento de pacientes em ambiente hospitalar utilizando ESP32 como gateway BLE para captura de eventos de proximidade.

Os eventos BLE coletados pelos dispositivos são enviados para a nuvem, armazenados na camada bruta e posteriormente transformados em camadas analíticas para consumo em dashboard.

---

## Arquitetura

```
ESP32 → Cloud Run → Pub/Sub → BigQuery raw → dbt (staging → intermediate → marts) → Looker Studio
```

Para detalhes completos do fluxo, contratos do evento e estrutura do BigQuery, consulte [`docs/arquitetura.md`](docs/arquitetura.md).

---

## Contrato inicial do evento

Exemplo de payload enviado pelo ESP32 para a API de ingestão:

```json
{
  "gateway_id": "gw-teste-esp32-01",
  "device_timestamp": "2026-03-25T20:30:00Z",
  "tag_mac": "7c:ec:79:47:73:62",
  "found": true,
  "rssi": -66
}
```

O Cloud Run enriquece esse payload com `ingested_at` antes de publicar no Pub/Sub. Detalhes completos em [`docs/arquitetura.md`](docs/arquitetura.md).

---

## Stack

| Camada | Tecnologia |
|---|---|
| Dispositivo | ESP32 como gateway BLE + tags BLE |
| Ingestão | Cloud Run (Python) |
| Mensageria | Google Cloud Pub/Sub |
| Data warehouse | Google BigQuery |
| Transformação | dbt Core |
| Orquestração local | Docker + docker-compose |
| Dashboard | Looker Studio |
| Versionamento | Git + GitHub |

---

## Estrutura planejada do repositório

```
projeto-it/
├── dbt/                        # Projeto dbt (transformações)
│   ├── models/
│   │   ├── staging/
│   │   ├── intermediate/
│   │   └── marts/
│   ├── macros/
│   ├── seeds/
│   └── tests/
├── docs/                       # Documentação do projeto
│   └── arquitetura.md
├── infra/                      # Ambiente Docker
│   ├── Dockerfile
│   └── docker-compose.yml
├── services/                   # Aplicações da plataforma
│   └── ingestion-api/          # API de ingestão (Cloud Run)
├── .env.example                # Variáveis de ambiente documentadas
├── dbt_project.yml
├── profiles.yml
└── requirements.txt
```

---

## Pré-requisitos

- [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- [Google Cloud SDK](https://cloud.google.com/sdk/docs/install)
- Projeto GCP criado com as seguintes APIs habilitadas:
  - BigQuery API
  - Cloud Run Admin API
  - Cloud Pub/Sub API
  - Cloud Build API
  - Artifact Registry API
- Service account do dbt com chave JSON gerada (veja configuração abaixo)

---

## Configuração do ambiente local

### 1. Clone o repositório

```bash
git clone https://github.com/GuilhermeBrevilato/projeto-it.git
cd projeto-it
```

### 2. Crie o arquivo `.env`

```bash
cp .env.example .env
```

Preencha as variáveis com os valores do seu projeto GCP:

```env
GCP_PROJECT=seu-projeto-gcp
BQ_LOCATION=US
BQ_DATASET_RAW=it_raw
BQ_DATASET_STAGING=it_staging
BQ_DATASET_INTERMEDIATE=it_intermediate
BQ_DATASET_MARTS=it_marts
GOOGLE_APPLICATION_CREDENTIALS=/caminho/local/para/sua-chave.json
```

### 3. Sobre o `profiles.yml`

O `profiles.yml` está versionado no repositório como parte do setup containerizado e reproduzível. Nenhum valor sensível é armazenado nele — as credenciais são injetadas via variáveis de ambiente.

- **Dentro do Docker:** o `profiles.yml` da raiz é usado diretamente, com `DBT_PROFILES_DIR=/mackenzie`.
- **Fora do Docker:** copie o `profiles.yml` para `~/.dbt/profiles.yml` e ajuste o caminho da chave JSON conforme seu ambiente local.

---

## Como rodar o dbt

O dbt é executado dentro de um container Docker. Todos os comandos passam pelo `docker-compose`.

### Validar a conexão com o BigQuery

```bash
docker compose --env-file .env -f infra/docker-compose.yml run --rm dbt dbt debug
```

### Rodar os modelos

```bash
docker compose --env-file .env -f infra/docker-compose.yml run --rm dbt dbt run
```

### Rodar os testes

```bash
docker compose --env-file .env -f infra/docker-compose.yml run --rm dbt dbt test
```

---

## Status validado até o momento

- ✅ Projeto GCP criado (`projeto-it-dev`)
- ✅ Datasets BigQuery provisionados (`it_raw`, `it_staging`, `it_intermediate`, `it_marts`)
- ✅ Tabela raw preparada para recebimento do payload bruto (`it_raw.esp32_payload`)
- ✅ Pub/Sub configurado (tópico + subscription gravando na raw)
- ✅ dbt local validado com BigQuery via Docker (`dbt debug` passou)

A próxima etapa é a implementação da camada de ingestão em Cloud Run.

---

## Roadmap

| Fase | Descrição | Status |
|---|---|---|
| Fase I | Fundação da plataforma | Concluída |
| Fase II | Camada de ingestão (Cloud Run + Pub/Sub) | Em andamento |
| Fase III | Modelos dbt (staging → intermediate → marts) | Pendente |
| Fase IV | Dashboard (Looker Studio) | Pendente |

---

## Contexto acadêmico

Projeto desenvolvido como trabalho prático na **Universidade Presbiteriana Mackenzie**, com objetivo de aplicar conceitos de engenharia de dados em um cenário real de IoT hospitalar.
