with eventos as (

    select * from {{ ref('int_ble_events') }}
    where found = true
      and is_valid_rssi = true
      and is_core_event_valid = true
      and grupo is not null
      and local is not null

),

-- Para cada evento, identifica qual gateway tem o maior RSSI
-- naquele momento — esse é o local dominante
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

-- Pega só o gateway dominante de cada momento
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

-- Detecta mudanças de local — quando o local atual é diferente do anterior
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

-- Marca o início de cada nova visita a um local
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

-- Cria um ID de sessão para cada visita contínua
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

-- Agrega cada sessão
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
        TIMESTAMP_DIFF(
            MAX(event_timestamp_utc),
            MIN(event_timestamp_utc),
            MINUTE
        )                                                       as minutos_no_local,
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