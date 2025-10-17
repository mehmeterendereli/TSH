cmake_minimum_required(VERSION 3.24)

project(TSHNativeDiagnostics
    VERSION 0.1.0
    DESCRIPTION "Native Windows memory introspection toolkit"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TSH_ENABLE_IMGUI "Enable Dear ImGui UI" OFF)
option(TSH_ENABLE_TESTS "Build unit and integration tests" ON)
option(TSH_ENABLE_RESEARCH_LOADER "Enable research driver loader" OFF)

add_subdirectory(src/user)
add_subdirectory(src/kernel)

if (TSH_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
