import logging
import os
from datetime import datetime, timezone
from typing import List

import psycopg2
import psycopg2.pool
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Validação de variáveis de ambiente no startup
# ---------------------------------------------------------------------------

POSTGRES_HOST = os.environ.get("POSTGRES_HOST", "localhost")
POSTGRES_PORT = os.environ.get("POSTGRES_PORT", "5432")
POSTGRES_DB   = os.environ.get("POSTGRES_DB")
POSTGRES_USER = os.environ.get("POSTGRES_USER")
POSTGRES_PASS = os.environ.get("POSTGRES_PASSWORD")
API_KEY       = os.environ.get("API_KEY")

if not POSTGRES_DB:
    raise ValueError("POSTGRES_DB não definida")
if not POSTGRES_USER:
    raise ValueError("POSTGRES_USER não definida")
if not POSTGRES_PASS:
    raise ValueError("POSTGRES_PASSWORD não definida")
if not API_KEY:
    raise ValueError("API_KEY não definida")

# ---------------------------------------------------------------------------
# Pool de conexões — criado uma vez no startup, reutilizado em cada request
# ---------------------------------------------------------------------------

connection_pool = psycopg2.pool.ThreadedConnectionPool(
    minconn=1,
    maxconn=10,
    host=POSTGRES_HOST,
    port=POSTGRES_PORT,
    dbname=POSTGRES_DB,
    user=POSTGRES_USER,
    password=POSTGRES_PASS,
)

logger.info("Pool de conexões PostgreSQL inicializado. Host: %s DB: %s", POSTGRES_HOST, POSTGRES_DB)

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


def insert_event(event: ESP32Event, ingested_at: str) -> None:
    """Insere um único evento na tabela it_raw.esp32_payload."""
    conn = connection_pool.getconn()
    try:
        with conn.cursor() as cur:
            cur.execute(
                """
                INSERT INTO it_raw.esp32_payload
                    (gateway_id, device_timestamp, tag_mac, found, rssi, ingested_at)
                VALUES
                    (%s, %s, %s, %s, %s, %s)
                """,
                (
                    event.gateway_id,
                    event.device_timestamp,
                    event.tag_mac,
                    event.found,
                    event.rssi,
                    ingested_at,
                ),
            )
        conn.commit()
    finally:
        connection_pool.putconn(conn)


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

    ingested_at = datetime.now(timezone.utc).isoformat()

    try:
        insert_event(event, ingested_at)
    except Exception as e:
        logger.error("Falha ao inserir no PostgreSQL: %s", e)
        raise HTTPException(status_code=500, detail=str(e))

    logger.info("Evento inserido. gateway_id=%s tag_mac=%s", event.gateway_id, event.tag_mac)

    return {
        "status": "ok",
        "ingested_at": ingested_at,
    }


@app.post("/ingest/batch")
async def ingest_batch(
    events: List[ESP32Event],
    x_api_key: str = Header(alias="X-API-Key"),
):
    if x_api_key != API_KEY:
        logger.warning("Requisição rejeitada: chave de API inválida.")
        raise HTTPException(status_code=401, detail="Chave de API inválida.")

    if not events:
        raise HTTPException(status_code=400, detail="Lista de eventos vazia.")

    if len(events) > 500:
        raise HTTPException(status_code=400, detail="Máximo de 500 eventos por lote.")

    inserted = 0
    failed = 0
    ingested_at = datetime.now(timezone.utc).isoformat()

    for event in events:
        try:
            insert_event(event, ingested_at)
            inserted += 1
        except Exception as e:
            logger.error("Falha ao inserir evento gateway_id=%s: %s", event.gateway_id, e)
            failed += 1

    logger.info("Batch concluído. inserted=%d failed=%d", inserted, failed)

    return {
        "status": "ok",
        "inserted": inserted,
        "failed": failed,
        "total": len(events),
    }
