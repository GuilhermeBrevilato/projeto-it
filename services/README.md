# ingestion-api

Serviço de ingestão HTTP do Projeto IT, responsável por receber eventos enviados por gateways ESP32, validar o payload, enriquecer a mensagem com timestamp de ingestão e publicar os dados no Google Cloud Pub/Sub.

Este serviço está implantado no Google Cloud Run e integra a camada inicial do pipeline de dados do projeto.

---

# Papel na arquitetura

```text
ESP32
→ ingestion-api (Cloud Run / FastAPI)
→ Pub/Sub
→ BigQuery Raw
→ dbt
→ Looker Studio
```

A `ingestion-api` é a porta de entrada dos eventos gerados na borda (IoT).

---

# Responsabilidades do serviço

- Receber eventos HTTP enviados pelos gateways ESP32
- Validar estrutura do payload com Pydantic
- Acrescentar campo `ingested_at`
- Publicar mensagem no Pub/Sub
- Retornar resposta HTTP ao cliente
- Servir endpoint de health check para observabilidade

---

# Stack utilizada

| Camada | Tecnologia |
|---|---|
| API | FastAPI |
| Runtime | Python 3.12 |
| Validação | Pydantic |
| Mensageria | Google Cloud Pub/Sub |
| Container | Docker |
| Deploy | Cloud Run |

---

# Estrutura do serviço

```text
services/ingestion-api/
├── app/
│   ├── __init__.py
│   └── main.py
├── .dockerignore
├── .env.example
├── Dockerfile
├── README.md
└── requirements.txt
```

---

# Endpoints disponíveis

## GET /health

Usado para health checks e validação operacional.

### Exemplo:

```bash
curl https://SEU_URL/health
```

### Resposta:

```json
{
  "status": "ok"
}
```

---

## POST /ingest

Recebe evento BLE e publica no Pub/Sub.

### Payload atual (v2)

```json
{
  "gateway_id": "gw-teste-esp32-01",
  "device_timestamp": "2026-04-25T16:35:00Z",
  "tag_mac": "7c:ec:79:47:73:62",
  "found": true,
  "rssi": -66
}
```

### Resposta esperada

```json
{
  "status": "ok",
  "ingested_at": "2026-04-28T12:05:47.185201+00:00"
}
```

---

# Evento publicado no Pub/Sub

Após enriquecimento, a API publica:

```json
{
  "ingested_at": "2026-04-28T12:05:47.185201+00:00",
  "gateway_id": "gw-teste-esp32-01",
  "device_timestamp": "2026-04-25T16:35:00Z",
  "tag_mac": "7c:ec:79:47:73:62",
  "found": true,
  "rssi": -66
}
```

---

# Variáveis de ambiente

```env
GCP_PROJECT=projeto-it-dev
PUBSUB_TOPIC_ID=ble-events-topic
```

## Descrição

| Variável | Função |
|---|---|
| GCP_PROJECT | Projeto GCP utilizado |
| PUBSUB_TOPIC_ID | Tópico onde eventos serão publicados |

---

# Execução local

## Instalar dependências

```bash
pip install -r requirements.txt
```

## Rodar aplicação

```bash
uvicorn app.main:app --reload
```

---

# Build Docker local

```bash
docker build -t ingestion-api .
```

---

# Deploy no Cloud Run

Fluxo utilizado no projeto:

```text
Docker build
→ Artifact Registry
→ Cloud Run revision
→ Rollout
```

---

# Caso real resolvido

Durante a implantação, a integração Pub/Sub → BigQuery apresentou erro `invalid_argument`.

Foi conduzido RCA (Root Cause Analysis), identificando incompatibilidade no formato do payload. O contrato foi refatorado para schema flat, restabelecendo a ingestão ponta a ponta.

---

# Troubleshooting comum

## /health responde, /ingest falha

Verificar:

- variáveis de ambiente
- permissões Pub/Sub
- nome do tópico
- logs do Cloud Run

## Cloud Run não sobe

Verificar:

- porta 8080
- comando do container
- dependências no requirements.txt

---

# Próximos passos

- autenticação entre gateways e API
- batch ingestion
- retries automáticos
- métricas de latência
- observabilidade avançada
- testes automatizados

---

# Autor

Guilherme Brevilato  
Projeto IT

