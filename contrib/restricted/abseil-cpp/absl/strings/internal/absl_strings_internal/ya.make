# Generated by devtools/yamaker.

LIBRARY()

WITHOUT_LICENSE_TEXTS()

OWNER(g:cpp-contrib)

LICENSE(Apache-2.0)

PEERDIR(
    contrib/restricted/abseil-cpp/absl/base 
    contrib/restricted/abseil-cpp/absl/base/internal/raw_logging
    contrib/restricted/abseil-cpp/absl/base/internal/spinlock_wait 
    contrib/restricted/abseil-cpp/absl/base/log_severity
)

ADDINCL(
    GLOBAL contrib/restricted/abseil-cpp
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DNOMINMAX
)

SRCDIR(contrib/restricted/abseil-cpp/absl/strings/internal)

SRCS(
    escaping.cc
    ostringstream.cc
    utf8.cc
)

END()
