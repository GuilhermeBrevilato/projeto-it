with eventos as (

    select * from {{ ref('int_ble_events') }}
    where grupo is not null

),

-- Para cada grupo e timestamp, verifica se foi detectado por algum gateway
deteccao_por_momento as (

    select
        event_timestamp_utc,
        event_date,
        grupo,
        tag_mac,
        MAX(case when found = true then 1 else 0 end) as detectado

    from eventos
    group by
        event_timestamp_utc,
        event_date,
        grupo,
        tag_mac

),

-- Calcula o tempo desde a última detecção
com_ultimo_sinal as (

    select
        event_timestamp_utc,
        event_date,
        grupo,
        tag_mac,
        detectado,
        lag(event_timestamp_utc) over (
            partition by grupo
            order by event_timestamp_utc
        ) as timestamp_anterior,
        lag(detectado) over (
            partition by grupo
            order by event_timestamp_utc
        ) as detectado_anterior

    from deteccao_por_momento

),

-- Identifica períodos de ausência maiores que 10 minutos
ausencias as (

    select
        event_date,
        grupo,
        tag_mac,
        timestamp_anterior        as inicio_ausencia,
        event_timestamp_utc       as fim_ausencia,
        TIMESTAMP_DIFF(
            event_timestamp_utc,
            timestamp_anterior,
            MINUTE
        )                         as minutos_ausente

    from com_ultimo_sinal
    where detectado = 1
      and detectado_anterior = 0
      and timestamp_anterior is not null
      and TIMESTAMP_DIFF(
            event_timestamp_utc,
            timestamp_anterior,
            MINUTE
          ) >= 10

)

select * from ausencias
order by event_date, grupo, inicio_ausencia