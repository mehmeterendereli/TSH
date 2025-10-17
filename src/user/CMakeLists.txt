set(TSH_USER_SOURCES
    App/Main.cpp
    Comms/DriverChannel.cpp
    Core/ProcessManager.cpp
    Core/MemoryAccessor.cpp
    Core/PatternScanner.cpp
    Core/PointerResolver.cpp
    Analysis/ResultRefiner.cpp
    Analysis/ScanSession.cpp
    Monitoring/ValueMonitor.cpp
    Instrumentation/HookController.cpp
)

add_executable(TSH.User WIN32 ${TSH_USER_SOURCES})

target_include_directories(TSH.User
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TSH.User
    PRIVATE
        kernel32.lib
        user32.lib
        shell32.lib
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
if (TSH_ENABLE_IMGUI)
    target_sources(TSH.User PRIVATE App/ImGuiShell.cpp)
    target_compile_definitions(TSH.User PRIVATE TSH_WITH_IMGUI)
    target_include_directories(TSH.User
        PRIVATE
            ${CMAKE_SOURCE_DIR}/external/imgui)
endif()
