#!/bin/bash
# Carrega variáveis do .env e roda o dbt
set -a
source /Users/guilhermebrevilato/Documents/projeto-it/.env
set +a

/Users/guilhermebrevilato/Documents/projeto-it/.venv/bin/dbt run \
  --project-dir /Users/guilhermebrevilato/Documents/projeto-it \
  --profiles-dir /Users/guilhermebrevilato/Documents/projeto-it \
  >> /Users/guilhermebrevilato/Documents/projeto-it/logs/dbt_cron.log 2>&1
