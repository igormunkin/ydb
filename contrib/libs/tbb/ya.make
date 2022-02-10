# Generated by devtools/yamaker from nixpkgs 32f7980afb5e33f1e078a51e715b9f102f396a69.

LIBRARY() 

OWNER(
    orivej
    g:cpp-contrib
)

VERSION(2021.2.0)

ORIGINAL_SOURCE(https://github.com/oneapi-src/oneTBB/archive/v2021.2.0.tar.gz)

LICENSE(Apache-2.0) 
 
LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

ADDINCL(
    GLOBAL contrib/libs/tbb/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -D__TBB_BUILD
)

SRCS(
    src/tbb/allocator.cpp 
    src/tbb/arena.cpp
    src/tbb/arena_slot.cpp 
    src/tbb/concurrent_bounded_queue.cpp 
    src/tbb/dynamic_link.cpp
    src/tbb/exception.cpp 
    src/tbb/global_control.cpp 
    src/tbb/governor.cpp
    src/tbb/main.cpp 
    src/tbb/market.cpp
    src/tbb/misc.cpp 
    src/tbb/misc_ex.cpp 
    src/tbb/observer_proxy.cpp
    src/tbb/parallel_pipeline.cpp 
    src/tbb/private_server.cpp
    src/tbb/profiling.cpp 
    src/tbb/queuing_rw_mutex.cpp
    src/tbb/rml_tbb.cpp 
    src/tbb/rtm_mutex.cpp 
    src/tbb/rtm_rw_mutex.cpp 
    src/tbb/semaphore.cpp
    src/tbb/small_object_pool.cpp 
    src/tbb/task.cpp
    src/tbb/task_dispatcher.cpp 
    src/tbb/task_group_context.cpp
    src/tbb/version.cpp 
)

IF (CLANG OR CLANG_CL)
    IF (ARCH_I386 OR ARCH_I686 OR ARCH_X86_64)
        CFLAGS(
            -mrtm
            -mwaitpkg
        )
    ENDIF()
ENDIF()

IF (OS_WINDOWS)
    CFLAGS(
        -DUSE_WINTHREAD
    )
ELSE()
    CFLAGS(
        -DUSE_PTHREAD
    )
ENDIF()

IF (GCC)
    CFLAGS(
        -flifetime-dse=1
        -mrtm
    )
ENDIF()

IF (NOT ARCH_ARM64)
    CFLAGS(
        -D__TBB_USE_ITT_NOTIFY 
        -DDO_ITT_NOTIFY
    )
    SRCS(
        src/tbb/itt_notify.cpp
    )
ENDIF()

END()
