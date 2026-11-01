cmake_minimum_required(VERSION 3.25)

project(MIPT_CACHE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_library(cache_strict_debug INTERFACE)
option(ENABLE_STRICT_DEBUG "Enable _GLIBCXX_DEBUG and Adress/UB sanitizers" OFF)

add_library(cache_compile_flags INTERFACE)

target_compile_options(cache_compile_flags INTERFACE
    $<$<CONFIG:DEBUG>:-Wall;-Wpedantic;-DDEBUG;-O0;-g;-Wmissing-declarations;-Wcast-align;-Wshadow;-Wsign-conversion>
    $<$<CONFIG:RELEASE>:-O2>
)

target_compile_options(cache_compile_flags INTERFACE
    $<$<CONFIG:DEBUG>:-O0 -g -DDEBUG>
    $<$<CONFIG:RELEASE>:-O3>
)

if(ENABLE_STRICT_DEBUG)
    target_compile_definitions(cache_strict_debug INTERFACE _GLIBCXX_DEBUG)
    target_compile_options(cache_strict_debug INTERFACE 
        -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(cache_strict_debug INTERFACE 
        -fsanitize=address,undefined
    )
endif()


option(ENABLE_UNIT_TESTS "Compile with UNIT_TESTS macro" ON)
if(ENABLE_UNIT_TESTS)
    target_compile_definitions(cache_compile_flags INTERFACE ENABLE_UNIT_TESTS=1)
endif()

add_subdirectory(modules)
add_subdirectory(tests)
