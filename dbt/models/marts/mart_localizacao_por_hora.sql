with eventos as (
    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true
      and grupo is not null
      and local is not null
),
rssi_por_hora as (
    select
        event_date,
        event_hour,
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        COUNT(*)                                    as total_registros,
        ROUND(AVG(rssi), 1)                         as rssi_medio,
        COUNT(*) FILTER (WHERE found = true)        as deteccoes
    from eventos
    group by
        event_date,
        event_hour,
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar
),
local_por_hora as (
    select
        event_date,
        event_hour,
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        total_registros,
        rssi_medio,
        deteccoes,
        row_number() over (
            partition by event_date, event_hour, grupo
            order by rssi_medio desc
        ) as rn
    from rssi_por_hora
)
select
    event_date,
    event_hour,
    grupo,
    tag_mac,
    gateway_id,
    local,
    andar,
    total_registros,
    rssi_medio,
    deteccoes
from local_por_hora
where rn = 1
order by event_date, grupo, event_hour
