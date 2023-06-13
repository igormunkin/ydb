# Generated by devtools/yamaker.

GTEST(benchmark_gtest)

WITHOUT_LICENSE_TEXTS()

PEERDIR(
    contrib/restricted/google/benchmark
)

ADDINCL(
    contrib/restricted/google/benchmark/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DBENCHMARK_STATIC_DEFINE
    -DGTEST_LINKED_AS_SHARED_LIBRARY=1
    -DHAVE_POSIX_REGEX
    -DHAVE_STD_REGEX
    -DHAVE_STEADY_CLOCK
)

SRCS(
    benchmark_gtest.cc
    benchmark_name_gtest.cc
    commandlineflags_gtest.cc
    statistics_gtest.cc
    string_util_gtest.cc
)

END()
