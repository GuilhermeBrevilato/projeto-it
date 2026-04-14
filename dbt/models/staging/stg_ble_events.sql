with source_data as (

    select * from {{ source('raw', 'esp32_payload') }}

),

renamed as (

    select
        coalesce(cast(event_id as string), concat(cast(gateway_id as string), '_', cast(device_timestamp as string), '_', cast(tag_mac as string))) as event_id,
        cast(gateway_id as string) as gateway_id,
        cast(sensor_id as string) as sensor_id,

        lower(
            regexp_replace(
                cast(tag_mac as string),
                r'[^0-9a-fA-F]',
                ''
            )
        ) as tag_mac_raw_normalized,

        safe_cast(rssi as int64) as rssi,
        safe_cast(found as bool) as found,
        safe_cast(device_timestamp as timestamp) as event_timestamp_utc,
        safe_cast(ingested_at as timestamp) as ingested_at,
        cast(raw_payload as string) as raw_payload

    from source_data

),

standardized as (

    select
        event_id,
        gateway_id,
        sensor_id,

        case
            when length(tag_mac_raw_normalized) = 12 then
                concat(
                    substr(tag_mac_raw_normalized, 1, 2), ':',
                    substr(tag_mac_raw_normalized, 3, 2), ':',
                    substr(tag_mac_raw_normalized, 5, 2), ':',
                    substr(tag_mac_raw_normalized, 7, 2), ':',
                    substr(tag_mac_raw_normalized, 9, 2), ':',
                    substr(tag_mac_raw_normalized, 11, 2)
                )
            else null
        end as tag_mac,

        rssi,
        found,
        event_timestamp_utc,
        ingested_at,
        raw_payload,

        date(event_timestamp_utc) as event_date,
        extract(hour from event_timestamp_utc) as event_hour,
        timestamp_trunc(event_timestamp_utc, minute) as event_minute,

        case
            when rssi between -120 and 0 then true
            else false
        end as is_valid_rssi,

        case
            when gateway_id is not null
             and event_timestamp_utc is not null
             and tag_mac_raw_normalized is not null
            then true
            else false
        end as is_core_event_valid

    from renamed

)

select * from standardized