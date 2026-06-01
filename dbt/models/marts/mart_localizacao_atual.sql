with eventos as (
    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true
),
ultimo_evento as (
    select
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        rssi,
        event_timestamp_utc,
        row_number() over (
            partition by grupo
            order by event_timestamp_utc desc
        ) as rn
    from eventos
),
localizacao_atual as (
    select
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        rssi,
        event_timestamp_utc as ultimo_sinal,
        EXTRACT(EPOCH FROM (current_timestamp - event_timestamp_utc))::INTEGER / 60
                            as minutos_desde_ultimo_sinal
    from ultimo_evento
    where rn = 1
      and grupo is not null
      and local is not null
)
select * from localizacao_atual
