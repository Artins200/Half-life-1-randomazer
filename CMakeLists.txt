cmake_minimum_required(VERSION 3.16)
project(hl1_randomizer_chaos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
    # Do not require a separately installed MSVC runtime for the shipped DLL.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CHAOS_COMMON_SOURCES
    src/main_dll.cpp
    src/chaos_events.cpp
    src/chaos_catalog.cpp
    src/chaos_features.cpp
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
    enable_testing()
    add_executable(test_chaos_simulation
        tests/test_chaos_simulation.cpp
        ${CHAOS_COMMON_SOURCES}
    )
    target_compile_definitions(test_chaos_simulation PRIVATE CHAOS_STANDALONE_SIMULATION _CRT_SECURE_NO_WARNINGS)
    target_include_directories(test_chaos_simulation PRIVATE include src)
    if(NOT WIN32)
        target_link_libraries(test_chaos_simulation PRIVATE m)
    endif()
    add_test(NAME chaos_simulation COMMAND test_chaos_simulation)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME opfor_client_setup COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_opfor_setup.py)
    endif()
    if(Python3_Interpreter_FOUND AND NOT WIN32)
        add_test(NAME linux_proxy_loader COMMAND ${CMAKE_COMMAND} -E env
            "CXX=${CMAKE_CXX_COMPILER}" ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_bridge_loader.py)
    elseif(Python3_Interpreter_FOUND AND WIN32)
        add_library(test_fake_original SHARED tests/fake_original_game.cpp tests/fake_original_win.def)
        target_include_directories(test_fake_original PRIVATE include)
        set_target_properties(test_fake_original PROPERTIES PREFIX "" OUTPUT_NAME "hl_orig")
        add_executable(test_win_loader tests/test_bridge_loader.cpp)
        target_include_directories(test_win_loader PRIVATE include)
        add_test(NAME windows_proxy_loader COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_bridge_loader_win.py
            $<TARGET_FILE:hl> $<TARGET_FILE:test_fake_original> $<TARGET_FILE:test_win_loader>)
        set_tests_properties(windows_proxy_loader PROPERTIES TIMEOUT 45)
    endif()
endif()
