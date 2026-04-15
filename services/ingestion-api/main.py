import json
import os
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from google.cloud import pubsub_v1

app = FastAPI()

class ESP32Event(BaseModel):
    gateway_id: str
    device_timestamp: str
    tag_mac: str
    found: bool
    rssi: int

PROJECT_ID = os.environ.get("GCP_PROJECT")
TOPIC_ID = os.environ.get("PUBSUB_TOPIC_ID")

@app.post("/ingest")
async def ingest(event: ESP32Event):

    # Passo 3 — Monta a mensagem interna da plataforma
    message = {
        "ingested_at": datetime.now(timezone.utc).isoformat(),
        "raw_payload": event.model_dump()
    }

    # Passo 4 — Publica no Pub/Sub
    try:
        publisher = pubsub_v1.PublisherClient()
        topic_path = publisher.topic_path(PROJECT_ID, TOPIC_ID)
        message_bytes = json.dumps(message).encode("utf-8")
        future = publisher.publish(topic_path, message_bytes)
        future.result()
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    return {"status": "ok", "ingested_at": message["ingested_at"]}