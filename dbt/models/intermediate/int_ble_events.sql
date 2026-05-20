with events as (

    select * from {{ ref('stg_ble_events') }}

),

beacons as (

    select * from {{ ref('beacons') }}

),

gateways as (

    select * from {{ ref('gateways') }}

),

enriched as (

    select
        -- identificação do evento
        e.gateway_id,
        e.tag_mac,

        -- localização física
        g.local,
        g.andar,

        -- grupo clínico
        b.grupo,

        -- sinal
        e.rssi,
        e.found,

        -- qualidade
        e.is_valid_rssi,
        e.is_core_event_valid,

        -- timestamps
        e.event_timestamp_utc,
        e.ingested_at,
        e.event_date,
        e.event_hour,
        e.event_minute

    from events as e
    left join beacons as b
        on e.tag_mac = b.tag_mac
    left join gateways as g
        on e.gateway_id = g.gateway_id

)

select * from enriched