LIBRARY()

SRCS(
    granule.cpp
    storage.cpp
)

PEERDIR(
    contrib/libs/apache/arrow
    ydb/core/protos
    ydb/core/formats/arrow
)

END()
