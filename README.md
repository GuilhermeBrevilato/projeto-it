# Projeto IT

Projeto de engenharia de dados e analytics voltado ao recebimento, armazenamento e transformação de eventos BLE coletados por dispositivos ESP32.

## Objetivo

Construir uma arquitetura de dados profissional para:

- receber eventos de telemetria BLE
- desacoplar ingestão e armazenamento
- preservar os eventos brutos em uma camada raw
- transformar os dados com dbt em camadas analíticas

## Arquitetura

Fluxo planejado do dado:

ESP32 → Cloud Run → Pub/Sub → BigQuery Raw → dbt → Staging / Intermediate / Marts

## Estrutura do projeto

- `dbt/`: modelos e estrutura analítica com dbt
- `infra/`: arquivos de infraestrutura local e execução com Docker
- `services/`: serviços de aplicação, como a API de ingestão
- `docs/`: documentação do projeto

## Configuração

O projeto utiliza:

- `.env.example` como modelo das variáveis necessárias
- `.env` local com os valores reais do ambiente
- `profiles.yml` parametrizado por variáveis de ambiente
- chave da service account fora do repositório

## Observações

Este repositório foi estruturado para desenvolvimento local com Docker e execução do dbt Core conectando ao BigQuery no GCP.