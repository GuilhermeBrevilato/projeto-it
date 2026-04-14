# Arquitetura do Projeto

## Visão geral

Sistema de rastreamento de pacientes em ambiente hospitalar utilizando sensores BLE e ESP32.
Os eventos de proximidade coletados pelos dispositivos são ingeridos em tempo real, transformados em camadas analíticas e consumidos via dashboard.

---

## Fluxo de dados

```
ESP32 → Cloud Run → Pub/Sub → BigQuery raw → dbt (staging → intermediate → marts) → Looker Studio
```

### Responsabilidade de cada camada

| Camada | Responsabilidade |
|---|---|
| ESP32 | Coleta eventos BLE e envia via HTTP para o Cloud Run |
| Cloud Run | Recebe o evento, adiciona metadados da plataforma e publica no Pub/Sub |
| Pub/Sub | Desacopla a ingestão do destino final e gerencia a entrega assíncrona das mensagens |
| BigQuery raw | Preserva o evento original sem transformação |
| dbt staging | Extrai e tipifica os campos do JSON cru |
| dbt intermediate | Aplica regras de negócio e enriquecimento |
| dbt marts | Gera tabelas analíticas prontas para consumo |
| Looker Studio | Visualização e dashboard para análise dos dados |

---

## Contratos do evento

### Contrato 1 — Payload externo do ESP32

Enviado pelo dispositivo via HTTP POST para o Cloud Run.

```json
{
  "gateway_id": "gw-teste-esp32-01",
  "device_timestamp": "2026-03-25T20:30:00Z",
  "tag_mac": "7c:ec:79:47:73:62",
  "found": true,
  "rssi": -66
}
```

| Campo | Tipo | Descrição |
|---|---|---|
| `gateway_id` | string | Identificador do gateway ESP32 |
| `device_timestamp` | string (ISO 8601) | Timestamp gerado pelo dispositivo |
| `tag_mac` | string | Endereço MAC da tag BLE detectada |
| `found` | boolean | Se a tag foi detectada (`true`) ou perdida (`false`) |
| `rssi` | integer | Intensidade do sinal BLE em dBm |

---

### Contrato 2 — Mensagem interna da plataforma

Publicada pelo Cloud Run no Pub/Sub. É o que chega na tabela `it_raw.esp32_payload`.

```json
{
  "ingested_at": "2026-03-25T20:30:01Z",
  "raw_payload": {
    "gateway_id": "gw-teste-esp32-01",
    "device_timestamp": "2026-03-25T20:30:00Z",
    "tag_mac": "7c:ec:79:47:73:62",
    "found": true,
    "rssi": -66
  }
}
```

| Campo | Tipo | Descrição |
|---|---|---|
| `ingested_at` | timestamp UTC | Momento em que a plataforma recebeu o evento |
| `raw_payload` | JSON | Payload original do ESP32, preservado sem modificação |

> **Por que separar `device_timestamp` de `ingested_at`?**
> O `device_timestamp` é gerado pelo dispositivo e pode ter drift de relógio ou atraso de rede.
> O `ingested_at` é gerado pela plataforma no momento da recepção e é confiável para ordenação temporal dos eventos.

---

## Estrutura do BigQuery

| Dataset | Camada | Descrição |
|---|---|---|
| `it_raw` | Raw | Eventos preservados como chegaram |
| `it_staging` | Staging | Campos extraídos, tipados e padronizados |
| `it_intermediate` | Intermediate | Lógica de negócio aplicada |
| `it_marts` | Marts | Tabelas analíticas prontas para o dashboard |

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
├── .gitignore
├── .dockerignore
├── dbt_project.yml
├── profiles.yml                # Versionado como parte do setup containerizado e reproduzível.
│                               # Valores sensíveis não são armazenados aqui;
│                               # são injetados por variáveis de ambiente via .env.
├── requirements.txt
└── README.md
```

---

## Infraestrutura GCP

| Serviço | Recurso | Função |
|---|---|---|
| Google Cloud Project | `projeto-it-dev` | Projeto GCP que agrupa todos os recursos da plataforma |
| Cloud Run | `ingestion-api` | API HTTP que recebe eventos do ESP32 |
| Pub/Sub | `ble-events-topic` | Tópico de mensageria |
| Pub/Sub | `ble-events-to-bq-raw-sub` | Subscription que grava na raw |
| BigQuery | `it_raw`, `it_staging`, `it_intermediate`, `it_marts` | Datasets da plataforma analítica |
| Artifact Registry | — | Armazena a imagem Docker da API de ingestão utilizada no deploy do Cloud Run |

---

## Service accounts

| Service account | Papel | Uso |
|---|---|---|
| `projeto-it-ingest@...` | Pub/Sub Publisher | Usada pelo Cloud Run para publicar eventos |
| `projeto-it-dbt@...` | BigQuery Job User + Data Editor | Usada pelo dbt para ler e escrever no BigQuery |
