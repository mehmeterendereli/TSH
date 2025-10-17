# Manual WDK override (ensure these paths exist locally)
set(WDK_INCLUDE_DIR "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/km" CACHE PATH "WDK include root" FORCE)
set(WDK_LIB_DIR     "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/km/x64" CACHE PATH "WDK library root" FORCE)
set(WDK_FOUND TRUE CACHE BOOL "WDK present flag" FORCE)

set(TSH_KERNEL_SOURCES
    DriverEntry.c
    Common/DriverUtils.c
    Memory/MemoryOps.c
    Scan/PatternScan.c
    Pointer/PointerTrace.c
    Monitor/Monitor.c
    Instrumentation/HookEngine.c
)

add_library(TSH.Driver SHARED ${TSH_KERNEL_SOURCES})

target_include_directories(TSH.Driver
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${WDK_INCLUDE_DIR}
)

target_compile_definitions(TSH.Driver
    PRIVATE
        TSH_KERNEL_BUILD
        NOMINMAX
        _AMD64_
        AMD64
        WIN64
        NTDDI_VERSION=0x0A00000B
        _WIN32_WINNT=0x0A00
        VER_PRODUCTBUILD=19041
)

if (MSVC)
    target_compile_options(TSH.Driver
        PRIVATE
            /kernel
            /Zl
            /Zp8
            /Gs
            /GS-

            /sdl-
    )

    target_link_directories(TSH.Driver PRIVATE "${WDK_LIB_DIR}")

    target_link_libraries(TSH.Driver
        PRIVATE
            ntoskrnl.lib
            hal.lib
            ksecdd.lib
            BufferOverflowK.lib
)

    target_link_options(TSH.Driver
        PRIVATE
            /NODEFAULTLIB
            /SUBSYSTEM:NATIVE
            /DRIVER
            /ENTRY:DriverEntry
            /MANIFEST:NO
    )
endif()

set_target_properties(TSH.Driver PROPERTIES
    OUTPUT_NAME "tsh_driver"
    SUFFIX ".sys"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/driver"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/driver"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/driver"
    LINKER_LANGUAGE C
)
