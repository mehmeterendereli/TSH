find_path(WDK_INCLUDE_DIR
    NAMES ntddk.h
    HINTS
        "$ENV{WDK_DIR}/Include"
        "C:/Program Files (x86)/Windows Kits/10/Include"
        "C:/Program Files/Windows Kits/10/Include"
    PATH_SUFFIXES
        km
        km/shared
        km/crt
        km/ddk
)

if (NOT WDK_INCLUDE_DIR)
    message(WARNING "Windows Driver Kit headers not located, skipping kernel driver build.")
    return()
endif()

set(TSH_KERNEL_SOURCES
    DriverEntry.c
    Common/DriverUtils.c
    Memory/MemoryOps.c
    Scan/PatternScan.c
    Pointer/PointerTrace.c
    Monitor/Monitor.c
    Instrumentation/HookEngine.c
)

add_library(TSH.Driver STATIC ${TSH_KERNEL_SOURCES})

target_include_directories(TSH.Driver
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${WDK_INCLUDE_DIR}
)

target_compile_definitions(TSH.Driver
    PRIVATE
        _KERNEL_MODE
        NOMINMAX
)

if (MSVC)
    target_compile_options(TSH.Driver
        PRIVATE
            /kernel
            /Zl
            /Zp8
            /Gs
    )
endif()

set_target_properties(TSH.Driver PROPERTIES
    OUTPUT_NAME "tsh_driver"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/driver"
)

add_custom_target(TSH.DriverPackage ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:TSH.Driver>
        ${CMAKE_BINARY_DIR}/dist/tsh_driver.lib
    DEPENDS TSH.Driver
    COMMENT "Preparing placeholder driver package artifact")
