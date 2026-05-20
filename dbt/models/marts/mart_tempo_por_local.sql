with eventos as (

    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true

),

-- Para cada grupo e local, calcula o tempo de permanência
-- usando a diferença entre o primeiro e o último sinal
tempo_por_local as (

    select
        event_date,
        grupo,
        local,
        andar,
        gateway_id,
        COUNT(*)                                                    as total_registros,
        MIN(event_timestamp_utc)                                    as primeiro_sinal,
        MAX(event_timestamp_utc)                                    as ultimo_sinal,
        TIMESTAMP_DIFF(
            MAX(event_timestamp_utc),
            MIN(event_timestamp_utc),
            MINUTE
        )                                                           as minutos_no_local,
        ROUND(AVG(rssi), 1)                                        as rssi_medio,
        MIN(rssi)                                                   as rssi_min,
        MAX(rssi)                                                   as rssi_max

    from eventos
    where grupo is not null
      and local is not null
    group by
        event_date,
        grupo,
        local,
        andar,
        gateway_id

)

select * from tempo_por_local
order by event_date, grupo, primeiro_sinal