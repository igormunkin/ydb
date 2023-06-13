LIBRARY()

IF (KIKIMR_DEFAULT_SHARDED_COMPACTION)
    # Makes it easier to test sharded compaction
    CFLAGS(
        -DKIKIMR_DEFAULT_SHARDED_COMPACTION=1
    )
ENDIF()

SRCS(
    actor_activity_names.cpp
    appdata.h
    appdata.cpp
    board_lookup.cpp
    board_publish.cpp
    board_replica.cpp
    blobstorage.h
    blobstorage.cpp
    channel_profiles.h
    counters.cpp
    counters.h
    defs.h
    domain.h
    event_filter.cpp
    event_filter.h
    events.h
    group_stat.cpp
    group_stat.h
    hive.h
    interconnect_channels.h
    kikimr_issue.cpp
    kikimr_issue.h
    localdb.cpp
    localdb.h
    location.h
    logoblob.cpp
    logoblob.h
    nameservice.h
    path.cpp
    pathid.cpp
    pool_stats_collector.cpp
    pool_stats_collector.h
    quoter.cpp
    quoter.h
    resource_profile.h
    row_version.cpp
    row_version.h
    services_assert.cpp
    shared_quota.h
    statestorage.cpp
    statestorage.h
    statestorage_event_filter.cpp
    statestorage_guardian.cpp
    statestorage_guardian_impl.h
    statestorage_impl.h
    statestorage_monitoring.cpp
    statestorage_proxy.cpp
    statestorage_replica.cpp
    statestorage_replica_probe.cpp
    statestorage_warden.cpp
    storage_pools.cpp
    storage_pools.h
    subdomain.h
    subdomain.cpp
    table_index.cpp
    tablet.cpp
    tablet.h
    tablet_killer.cpp
    tablet_pipe.h
    tablet_pipecache.h
    tablet_resolver.h
    tablet_status_checker.cpp
    tabletid.h
    tablet_types.h
    traceid.cpp
    traceid.h
    tracing.h
    tx_processing.h
    tx_processing.cpp
    user_registry.h
    blobstorage_grouptype.cpp
    wilson.h
)

PEERDIR(
    library/cpp/actors/core
    library/cpp/actors/helpers
    library/cpp/actors/interconnect
    library/cpp/actors/protos
    library/cpp/actors/wilson
    library/cpp/deprecated/enum_codegen
    library/cpp/logger
    library/cpp/lwtrace
    library/cpp/lwtrace/mon
    library/cpp/random_provider
    library/cpp/time_provider
    ydb/core/base/services
    ydb/core/debug
    ydb/core/erasure
    ydb/core/protos
    ydb/core/protos/out
    ydb/library/aclib
    ydb/library/login
    ydb/library/pdisk_io
    ydb/library/pretty_types_print/protobuf
    ydb/public/api/protos/out
    ydb/library/yql/minikql
    library/cpp/deprecated/atomic
)

RESOURCE(
    ydb/core/base/kikimr_issue.txt kikimr_issue.txt
)

GENERATE_ENUM_SERIALIZATION(quoter.h)

END()

RECURSE_FOR_TESTS(
    ut
    ut_board_subscriber
)
