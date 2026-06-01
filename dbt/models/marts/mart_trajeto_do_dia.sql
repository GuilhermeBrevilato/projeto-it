with eventos as (
    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true
      and grupo is not null
      and local is not null
),
rssi_por_momento as (
    select
        event_timestamp_utc,
        event_date,
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        rssi,
        row_number() over (
            partition by grupo, event_timestamp_utc
            order by rssi desc
        ) as rn_rssi
    from eventos
),
local_dominante as (
    select
        event_timestamp_utc,
        event_date,
        grupo,
        tag_mac,
        gateway_id,
        local,
        andar,
        rssi
    from rssi_por_momento
    where rn_rssi = 1
),
com_local_anterior as (
    select
        event_timestamp_utc,
        event_date,
        grupo,
        local,
        andar,
        gateway_id,
        rssi,
        lag(local) over (
            partition by grupo, event_date
            order by event_timestamp_utc
        ) as local_anterior
    from local_dominante
),
mudancas as (
    select
        event_timestamp_utc,
        event_date,
        grupo,
        local,
        andar,
        gateway_id,
        rssi,
        case
            when local != local_anterior or local_anterior is null then 1
            else 0
        end as nova_visita
    from com_local_anterior
),
sessoes as (
    select
        event_timestamp_utc,
        event_date,
        grupo,
        local,
        andar,
        gateway_id,
        rssi,
        sum(nova_visita) over (
            partition by grupo, event_date
            order by event_timestamp_utc
            rows unbounded preceding
        ) as sessao_id
    from mudancas
),
trajeto as (
    select
        event_date,
        grupo,
        sessao_id,
        local,
        andar,
        gateway_id,
        MIN(event_timestamp_utc)                                as chegada,
        MAX(event_timestamp_utc)                                as saida,
        EXTRACT(EPOCH FROM (
            MAX(event_timestamp_utc) - MIN(event_timestamp_utc)
        ))::INTEGER / 60                                        as minutos_no_local,
        COUNT(*)                                                as total_registros,
        ROUND(AVG(rssi), 1)                                     as rssi_medio
    from sessoes
    group by
        event_date,
        grupo,
        sessao_id,
        local,
        andar,
        gateway_id
)
select * from trajeto
order by event_date, grupo, sessao_id
