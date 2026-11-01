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
    $<$<CONFIG:DEBUG>:-Wall;>
    $<$<CONFIG:RELEASE>:-O2>
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


add_subdirectory(modules)
add_subdirectory(tests)
