cmake_minimum_required(VERSION 3.16)
project(hl1_randomizer_chaos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CHAOS_COMMON_SOURCES
    src/main_dll.cpp
    src/chaos_events.cpp
    src/death_counter.cpp
)

if(WIN32)
    add_library(hl SHARED
        ${CHAOS_COMMON_SOURCES}
        src/dll_bridge_win32.cpp
        src/hl.def
    )
    target_compile_definitions(hl PRIVATE WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS)
    set_target_properties(hl PROPERTIES
        PREFIX ""
        OUTPUT_NAME "hl"
        SUFFIX ".dll"
    )
else()
    add_library(hl SHARED
        ${CHAOS_COMMON_SOURCES}
        src/dll_bridge_linux.cpp
    )
    target_compile_options(hl PRIVATE -fPIC -fvisibility=hidden)
    target_link_libraries(hl PRIVATE dl m)
    set_target_properties(hl PROPERTIES
        PREFIX ""
        OUTPUT_NAME "hl"
        SUFFIX ".so"
    )
endif()

target_include_directories(hl PRIVATE include src)

option(BUILD_CHAOS_TESTS "Build standalone GoldSrc chaos simulation test" ON)
if(BUILD_CHAOS_TESTS)
    add_executable(test_chaos_simulation
        tests/test_chaos_simulation.cpp
        ${CHAOS_COMMON_SOURCES}
    )
    target_compile_definitions(test_chaos_simulation PRIVATE CHAOS_STANDALONE_SIMULATION _CRT_SECURE_NO_WARNINGS)
    target_include_directories(test_chaos_simulation PRIVATE include src)
    if(NOT WIN32)
        target_link_libraries(test_chaos_simulation PRIVATE m)
    endif()
endif()
