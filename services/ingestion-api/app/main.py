import decimal
import logging
import os
from datetime import datetime, timezone, date as date_type
from typing import List

import psycopg2
import psycopg2.extras
import psycopg2.pool
from fastapi import FastAPI, Header, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Variáveis de ambiente
# ---------------------------------------------------------------------------

POSTGRES_HOST = os.environ.get("POSTGRES_HOST", "localhost")
POSTGRES_PORT = os.environ.get("POSTGRES_PORT", "5432")
POSTGRES_DB   = os.environ.get("POSTGRES_DB")
POSTGRES_USER = os.environ.get("POSTGRES_USER")
POSTGRES_PASS = os.environ.get("POSTGRES_PASSWORD")
API_KEY       = os.environ.get("API_KEY")

if not POSTGRES_DB:   raise ValueError("POSTGRES_DB não definida")
if not POSTGRES_USER: raise ValueError("POSTGRES_USER não definida")
if not POSTGRES_PASS: raise ValueError("POSTGRES_PASSWORD não definida")
if not API_KEY:       raise ValueError("API_KEY não definida")

# ---------------------------------------------------------------------------
# Pool de conexões
# ---------------------------------------------------------------------------

connection_pool = psycopg2.pool.ThreadedConnectionPool(
    minconn=1, maxconn=10,
    host=POSTGRES_HOST, port=POSTGRES_PORT,
    dbname=POSTGRES_DB, user=POSTGRES_USER, password=POSTGRES_PASS,
)
logger.info("Pool de conexões PostgreSQL inicializado. Host: %s DB: %s", POSTGRES_HOST, POSTGRES_DB)

# ---------------------------------------------------------------------------
# Aplicação
# ---------------------------------------------------------------------------

app = FastAPI(title="Projeto IT Ingestion API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _serialize(row: dict) -> dict:
    out = {}
    for k, v in row.items():
        if isinstance(v, (datetime, date_type)):
            out[k] = v.isoformat()
        elif isinstance(v, decimal.Decimal):
            out[k] = float(v)
        else:
            out[k] = v
    return out


def db_query(sql: str, params=None) -> list:
    conn = connection_pool.getconn()
    try:
        with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cur:
            cur.execute(sql, params)
            return [_serialize(dict(r)) for r in cur.fetchall()]
    finally:
        connection_pool.putconn(conn)


# ---------------------------------------------------------------------------
# Modelo de ingestão
# ---------------------------------------------------------------------------

class ESP32Event(BaseModel):
    gateway_id: str
    device_timestamp: datetime
    tag_mac: str
    found: bool
    rssi: int


def insert_event(event: ESP32Event, ingested_at: str) -> None:
    conn = connection_pool.getconn()
    try:
        with conn.cursor() as cur:
            cur.execute(
                """
                INSERT INTO it_raw.esp32_payload
                    (gateway_id, device_timestamp, tag_mac, found, rssi, ingested_at)
                VALUES (%s, %s, %s, %s, %s, %s)
                """,
                (event.gateway_id, event.device_timestamp, event.tag_mac,
                 event.found, event.rssi, ingested_at),
            )
        conn.commit()
    finally:
        connection_pool.putconn(conn)


# ---------------------------------------------------------------------------
# Endpoints de ingestão
# ---------------------------------------------------------------------------

@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/ingest")
async def ingest(event: ESP32Event, x_api_key: str = Header(alias="X-API-Key")):
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
    return {"status": "ok", "ingested_at": ingested_at}


@app.post("/ingest/batch")
async def ingest_batch(events: List[ESP32Event], x_api_key: str = Header(alias="X-API-Key")):
    if x_api_key != API_KEY:
        logger.warning("Requisição rejeitada: chave de API inválida.")
        raise HTTPException(status_code=401, detail="Chave de API inválida.")
    if not events:
        raise HTTPException(status_code=400, detail="Lista de eventos vazia.")
    if len(events) > 500:
        raise HTTPException(status_code=400, detail="Máximo de 500 eventos por lote.")
    inserted, failed = 0, 0
    ingested_at = datetime.now(timezone.utc).isoformat()
    for event in events:
        try:
            insert_event(event, ingested_at)
            inserted += 1
        except Exception as e:
            logger.error("Falha ao inserir evento gateway_id=%s: %s", event.gateway_id, e)
            failed += 1
    logger.info("Batch concluído. inserted=%d failed=%d", inserted, failed)
    return {"status": "ok", "inserted": inserted, "failed": failed, "total": len(events)}


# ---------------------------------------------------------------------------
# Endpoints do dashboard (leitura dos marts)
# ---------------------------------------------------------------------------

@app.get("/api/localizacao-atual")
def api_localizacao_atual():
    return db_query("SELECT * FROM it_marts.mart_localizacao_atual ORDER BY grupo")


@app.get("/api/rssi-por-hora")
def api_rssi_por_hora():
    return db_query("""
        SELECT event_hour, grupo, rssi_medio, local, deteccoes
        FROM it_marts.mart_localizacao_por_hora
        ORDER BY event_hour, grupo
    """)


@app.get("/api/tempo-por-local")
def api_tempo_por_local():
    return db_query("""
        SELECT grupo, local, SUM(minutos_no_local) as minutos_total
        FROM it_marts.mart_tempo_por_local
        GROUP BY grupo, local
        ORDER BY grupo, minutos_total DESC
    """)


@app.get("/api/ausencias")
def api_ausencias():
    return db_query("""
        SELECT grupo, inicio_ausencia, fim_ausencia, minutos_ausente
        FROM it_marts.mart_ausencia_beacon
        ORDER BY inicio_ausencia DESC
        LIMIT 20
    """)


@app.get("/api/stats")
def api_stats():
    rows = db_query("""
        SELECT
            COUNT(*)                        AS total_eventos,
            MAX(ingested_at)                AS ultimo_evento
        FROM it_raw.esp32_payload
    """)
    return rows[0] if rows else {}

@app.get("/api/trajeto")
def api_trajeto():
    return db_query("""
        SELECT grupo, local, andar, chegada, saida,
               minutos_no_local, sessao_id
        FROM it_marts.mart_trajeto_do_dia
        WHERE event_date = CURRENT_DATE
        ORDER BY grupo, chegada
    """)

@app.get("/api/trajeto-replay")
def api_trajeto_replay():
    return db_query("""
        SELECT
            e.grupo,
            e.local,
            e.andar,
            e.event_timestamp_utc,
            e.rssi,
            e.gateway_id
        FROM it_intermediate.int_ble_events e
        WHERE e.found = true
          AND e.is_valid_rssi = true
          AND e.grupo IS NOT NULL
          AND e.local IS NOT NULL
          AND e.event_timestamp_utc >= current_timestamp - interval '15 minutes'
        ORDER BY e.event_timestamp_utc ASC
    """)