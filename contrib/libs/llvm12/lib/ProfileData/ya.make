# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/Demangle
    contrib/libs/llvm12/lib/IR
    contrib/libs/llvm12/lib/Support
)

ADDINCL(
    contrib/libs/llvm12/lib/ProfileData
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    GCOV.cpp
    InstrProf.cpp
    InstrProfReader.cpp
    InstrProfWriter.cpp
    ProfileSummaryBuilder.cpp
    SampleProf.cpp
    SampleProfReader.cpp
    SampleProfWriter.cpp
)

END()
