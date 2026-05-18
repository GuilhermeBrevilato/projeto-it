import json
import logging
import os
from datetime import datetime, timezone

from fastapi import FastAPI, Header, HTTPException
from google.cloud import pubsub_v1
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Validação de variáveis de ambiente no startup
# ---------------------------------------------------------------------------

PROJECT_ID = os.environ.get("GCP_PROJECT")
TOPIC_ID = os.environ.get("PUBSUB_TOPIC_ID")
API_KEY = os.environ.get("API_KEY")

if not PROJECT_ID:
    logger.error("Variável de ambiente GCP_PROJECT não definida. Encerrando.")
    raise ValueError("GCP_PROJECT não definida")

if not TOPIC_ID:
    logger.error("Variável de ambiente PUBSUB_TOPIC_ID não definida. Encerrando.")
    raise ValueError("PUBSUB_TOPIC_ID não definida")

if not API_KEY:
    logger.error("Variável de ambiente API_KEY não definida. Encerrando.")
    raise ValueError("API_KEY não definida")

# ---------------------------------------------------------------------------
# Publisher instanciado uma vez, fora de qualquer endpoint
# Usa ADC (Application Default Credentials):
#   - localmente: lê GOOGLE_APPLICATION_CREDENTIALS do ambiente
#   - no Cloud Run: usa a service account anexada ao serviço automaticamente
# ---------------------------------------------------------------------------

publisher = pubsub_v1.PublisherClient()
topic_path = publisher.topic_path(PROJECT_ID, TOPIC_ID)

logger.info("Publisher Pub/Sub inicializado. Topic: %s", topic_path)

# ---------------------------------------------------------------------------
# Aplicação
# ---------------------------------------------------------------------------

app = FastAPI(title="Projeto IT Ingestion API")


class ESP32Event(BaseModel):
    gateway_id: str
    device_timestamp: datetime
    tag_mac: str
    found: bool
    rssi: int


@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/ingest")
async def ingest(
    event: ESP32Event,
    x_api_key: str = Header(alias="X-API-Key"),
):
    if x_api_key != API_KEY:
        logger.warning("Requisição rejeitada: chave de API inválida.")
        raise HTTPException(status_code=401, detail="Chave de API inválida.")

    message = {
        "ingested_at": datetime.now(timezone.utc).isoformat(),
        "gateway_id": event.gateway_id,
        "device_timestamp": event.device_timestamp.isoformat(),
        "tag_mac": event.tag_mac,
        "found": event.found,
        "rssi": event.rssi,
    }

    try:
        message_bytes = json.dumps(message).encode("utf-8")
        future = publisher.publish(topic_path, message_bytes)
        future.result()
    except Exception as e:
        logger.error("Falha ao publicar no Pub/Sub: %s", e)
        raise HTTPException(status_code=500, detail=str(e))

    logger.info("Evento publicado. gateway_id=%s tag_mac=%s", event.gateway_id, event.tag_mac)

    return {
        "status": "ok",
        "ingested_at": message["ingested_at"],
    }