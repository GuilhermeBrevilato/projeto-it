import json
import os
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException
from google.cloud import pubsub_v1
from pydantic import BaseModel

app = FastAPI(title="Projeto IT Ingestion API")

PROJECT_ID = os.environ.get("GCP_PROJECT")
TOPIC_ID = os.environ.get("PUBSUB_TOPIC_ID")

if not PROJECT_ID:
    raise ValueError("Variável de ambiente GCP_PROJECT não definida")

if not TOPIC_ID:
    raise ValueError("Variável de ambiente PUBSUB_TOPIC_ID não definida")

publisher = pubsub_v1.PublisherClient()
topic_path = publisher.topic_path(PROJECT_ID, TOPIC_ID)


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
async def ingest(event: ESP32Event):
    message = {
        "ingested_at": datetime.now(timezone.utc).isoformat(),
        "raw_payload": event.model_dump(mode="json")
    }

    try:
        message_bytes = json.dumps(message).encode("utf-8")
        future = publisher.publish(topic_path, message_bytes)
        future.result()
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    return {
        "status": "ok",
        "ingested_at": message["ingested_at"]
    }