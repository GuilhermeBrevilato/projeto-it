with source as (

    select * from {{ source('raw', 'esp32_payload') }}

),

renamed as (

    select
        -- identificação
        gateway_id,
        tag_mac,

        -- sinal
        rssi,
        found,

        -- timestamps
        device_timestamp                                    as event_timestamp_utc,
        ingested_at,

        -- campos derivados
        date(device_timestamp)                             as event_date,
        extract(hour from device_timestamp)                as event_hour,
        timestamp_trunc(device_timestamp, minute)          as event_minute,

        -- qualidade do registro
        case
            when rssi between -120 and 0 then true
            else false
        end                                                as is_valid_rssi,

        case
            when gateway_id is not null
             and device_timestamp is not null
             and tag_mac is not null
            then true
            else false
        end                                                as is_core_event_valid

    from source

)

select * from renamed
