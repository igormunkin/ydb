YQL_UDF_YDB(json_udf)

YQL_ABI_VERSION(
    2
    28
    0
)

SRCS(
    json_udf.cpp
)

PEERDIR(
    library/cpp/json/easy_parse
)

END()

RECURSE_FOR_TESTS(
    test
)