with eventos as (

    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true

),

-- Para cada grupo e gateway, pega o evento mais recente
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

-- Pega só o registro mais recente de cada grupo
localizacao_atual as (

    select
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        rssi,
        event_timestamp_utc as ultimo_sinal,
        timestamp_diff(current_timestamp(), event_timestamp_utc, minute) as minutos_desde_ultimo_sinal

    from ultimo_evento
    where rn = 1
      and grupo is not null  -- ← filtra beacons não cadastrados
      and local is not null  -- ← filtra gateways não cadastrados

)

select * from localizacao_atual
