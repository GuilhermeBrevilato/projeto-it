-- =============================================================================
-- Migration 001: Schema it_raw e tabela esp32_payload
-- =============================================================================

-- Schema
CREATE SCHEMA IF NOT EXISTS it_raw;

-- Tabela principal de eventos BLE
CREATE TABLE IF NOT EXISTS it_raw.esp32_payload (
    id               BIGSERIAL    PRIMARY KEY,
    ingested_at      TIMESTAMPTZ  NOT NULL,
    gateway_id       TEXT         NOT NULL,
    device_timestamp TIMESTAMPTZ  NOT NULL,
    tag_mac          TEXT         NOT NULL,
    found            BOOLEAN      NOT NULL,
    rssi             INTEGER      NOT NULL
);

-- Índices para as queries mais comuns no dbt
CREATE INDEX IF NOT EXISTS idx_esp32_device_timestamp
    ON it_raw.esp32_payload (device_timestamp);

CREATE INDEX IF NOT EXISTS idx_esp32_gateway_id
    ON it_raw.esp32_payload (gateway_id);

CREATE INDEX IF NOT EXISTS idx_esp32_tag_mac
    ON it_raw.esp32_payload (tag_mac);
