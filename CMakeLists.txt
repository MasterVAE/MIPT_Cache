cmake_minimum_required(VERSION 3.25)

project(MIPT_CACHE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(ENABLE_ASAN "Enable AddressSanitizer for Debug builds" OFF)

option(ENABLE_DEBUG "Enable _GLIBCXX_DEBUG and sanitizers" OFF)

if(ENABLE_DEBUG)
    add_compile_definitions(_GLIBCXX_DEBUG)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(modules)
add_subdirectory(tests)
