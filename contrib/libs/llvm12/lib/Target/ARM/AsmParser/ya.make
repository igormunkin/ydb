# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/MC
    contrib/libs/llvm12/lib/MC/MCParser
    contrib/libs/llvm12/lib/Support
    contrib/libs/llvm12/lib/Target/ARM/MCTargetDesc
    contrib/libs/llvm12/lib/Target/ARM/TargetInfo
    contrib/libs/llvm12/lib/Target/ARM/Utils
)

ADDINCL(
    ${ARCADIA_BUILD_ROOT}/contrib/libs/llvm12/lib/Target/ARM
    contrib/libs/llvm12/lib/Target/ARM
    contrib/libs/llvm12/lib/Target/ARM/AsmParser
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    ARMAsmParser.cpp
)

END()
