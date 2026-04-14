# projeto-it

Sistema de rastreamento de pacientes em ambiente hospitalar utilizando sensores BLE e ESP32.

Os eventos de proximidade coletados pelos dispositivos são ingeridos em tempo real, transformados em camadas analíticas e consumidos via dashboard.

---

## Arquitetura

```
ESP32 → Cloud Run → Pub/Sub → BigQuery raw → dbt (staging → intermediate → marts) → Looker Studio
```

Para detalhes completos do fluxo, contratos do evento e estrutura do BigQuery, consulte [`docs/arquitetura.md`](docs/arquitetura.md).

---

## Stack

| Camada | Tecnologia |
|---|---|
| Dispositivo | ESP32 + sensores BLE |
| Ingestão | Cloud Run (Python) |
| Mensageria | Google Cloud Pub/Sub |
| Data warehouse | Google BigQuery |
| Transformação | dbt Core |
| Orquestração local | Docker + docker-compose |
| Dashboard | Looker Studio |
| Versionamento | Git + GitHub |

---

## Estrutura do repositório

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

### 3. Configure o `profiles.yml`

O `profiles.yml` na raiz do projeto é um template. Para uso local fora do Docker, copie-o para `~/.dbt/profiles.yml` e ajuste o caminho da chave.

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

## Status do projeto

| Fase | Descrição | Status |
|---|---|---|
| Fase I | Fundação da plataforma | Concluída |
| Fase II | Camada de ingestão (Cloud Run + Pub/Sub) | Em andamento |
| Fase III | Modelos dbt (staging → intermediate → marts) | Pendente |
| Fase IV | Dashboard (Looker Studio) | Pendente |

---

## Contexto acadêmico

Projeto desenvolvido como trabalho prático na **Universidade Presbiteriana Mackenzie**, com objetivo de aplicar conceitos de engenharia de dados em um cenário real de IoT hospitalar.
