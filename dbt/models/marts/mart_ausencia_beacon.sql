with eventos as (
    select * from {{ ref('int_ble_events') }}
    where grupo is not null
),
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
ausencias as (
    select
        event_date,
        grupo,
        tag_mac,
        timestamp_anterior        as inicio_ausencia,
        event_timestamp_utc       as fim_ausencia,
        EXTRACT(EPOCH FROM (event_timestamp_utc - timestamp_anterior))::INTEGER / 60
                                  as minutos_ausente
    from com_ultimo_sinal
    where detectado = 1
      and detectado_anterior = 0
      and timestamp_anterior is not null
      and EXTRACT(EPOCH FROM (event_timestamp_utc - timestamp_anterior))::INTEGER / 60 >= 10
)
select * from ausencias
order by event_date, grupo, inicio_ausencia
