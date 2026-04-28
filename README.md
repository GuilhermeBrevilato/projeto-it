# Projeto IT — Engenharia de Dados aplicada ao setor hospitalar

Projeto prático de Engenharia de Dados voltado ao rastreamento de deslocamento de pacientes em ambientes hospitalares utilizando sensores BLE e arquitetura cloud no Google Cloud Platform.

O objetivo é capturar eventos de proximidade em tempo real, estruturar um pipeline moderno de ingestão e transformar os dados em informações estratégicas para melhoria de fluxo interno, redução de congestionamentos e apoio à experiência do paciente.

---

# Problema de negócio

Hospitais possuem fluxos complexos de deslocamento:

- pacientes buscando setores
- filas internas
- pontos de congestionamento
- dificuldade de wayfinding (orientação interna)
- baixa visibilidade operacional em tempo real

Sem dados estruturados, oportunidades de melhoria ficam invisíveis.

---

# Solução proposta

Utilização de dispositivos ESP32 como gateways BLE para capturar sinais emitidos por tags/pulseiras e enviar eventos para uma arquitetura moderna de dados em nuvem.

Esses eventos são ingeridos, armazenados e transformados para análises operacionais e dashboards gerenciais.

---

# Arquitetura atual

```text
ESP32
→ Cloud Run (FastAPI)
→ Pub/Sub
→ BigQuery Raw
→ dbt (staging / intermediate / marts)
→ Looker Studio
```

---

# Stack utilizada

| Camada | Tecnologia |
|---|---|
| Edge / IoT | ESP32 + BLE |
| API de Ingestão | Python + FastAPI |
| Containerização | Docker |
| Deploy | Cloud Run |
| Mensageria | Pub/Sub |
| Data Warehouse | BigQuery |
| Transformação | dbt Core |
| Visualização | Looker Studio |
| Versionamento | Git + GitHub |

---

# Pipeline validado em produção

Atualmente o projeto já possui componentes reais implantados e funcionando:

- API containerizada publicada no Cloud Run
- Endpoint `/health` validado
- Endpoint `/ingest` validado
- Publicação em tópico Pub/Sub
- Subscription escrevendo no BigQuery
- Eventos ingeridos com sucesso na camada raw
- Deploy versionado com revisions e rollout controlado

---

# Caso real resolvido (RCA)

Durante a implementação, a integração Pub/Sub → BigQuery apresentou erro `invalid_argument`.

Após investigação ponta a ponta (Root Cause Analysis), o contrato do evento foi refatorado de payload aninhado para schema flat, restabelecendo a ingestão com sucesso.

Isso simulou um cenário real de troubleshooting em pipelines produtivos.

---

# Estrutura atual do repositório

```text
projeto-it/
├── README.md
├── docs/
├── services/
│   └── ingestion-api/
├── dbt/
├── infra/
└── ...
```

---

# Próximos passos

- Construção da camada staging no dbt
- Modelagem analítica de deslocamento hospitalar
- Dashboards operacionais
- Heatmaps de movimentação
- Métricas de tempo entre captura e ingestão
- Alertas operacionais

---

# Diferenciais do projeto

- Aplicação em problema real do setor hospitalar
- Arquitetura moderna baseada em eventos
- Experiência prática com cloud pública
- Integração entre IoT + Data Engineering
- Versionamento e deploy profissional
- Troubleshooting real de pipeline

---

# Sobre o autor

Guilherme Brevilato é profissional com experiência prévia no setor hospitalar e trajetória em engenharia, atualmente direcionando carreira para Engenharia de Dados.

Este projeto representa a união entre conhecimento operacional de saúde e tecnologia de dados aplicada a problemas reais.

---

# Contato

LinkedIn: inserir-linkedin  
GitHub: https://github.com/GuilhermeBrevilato

