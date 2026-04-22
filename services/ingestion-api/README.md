# ingestion-api

API de ingestão do Projeto IT, responsável por receber eventos enviados pelo gateway ESP32, validar o payload e publicar a mensagem no Google Cloud Pub/Sub.

---

## Papel na arquitetura

Esta aplicação atende à camada de ingestão do projeto:

ESP32 → Cloud Run → Pub/Sub → BigQuery raw

Sua função é servir como porta de entrada HTTP da plataforma, recebendo o evento do dispositivo, acrescentando o timestamp de ingestão e encaminhando a mensagem para a mensageria.

---

## Responsabilidades da API

- receber eventos HTTP enviados pelo ESP32
- validar o payload conforme o contrato da aplicação
- enriquecer a mensagem com `ingested_at`
- publicar a mensagem no Pub/Sub
- responder o status da operação ao cliente

---

## Estrutura do serviço

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