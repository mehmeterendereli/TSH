set(TSH_USER_SOURCES
    App/Main.cpp
    Comms/DriverChannel.cpp
    Core/ProcessManager.cpp
    Core/PatternScanner.cpp
    Core/PointerResolver.cpp
    Analysis/ResultRefiner.cpp
    Analysis/ScanSession.cpp
    Monitoring/ValueMonitor.cpp
    Instrumentation/HookController.cpp
)

add_executable(TSH.User ${TSH_USER_SOURCES})

target_include_directories(TSH.User
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TSH.User
    PRIVATE
        kernel32.lib
        user32.lib
)

target_compile_definitions(TSH.User
    PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
)

set_target_properties(TSH.User PROPERTIES
    OUTPUT_NAME "tsh_user"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
