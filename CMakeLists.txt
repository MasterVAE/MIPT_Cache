cmake_minimum_required(VERSION 3.25)

project(MIPT_CACHE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(ENABLE_ASAN "Enable AddressSanitizer for Debug builds" OFF)

add_subdirectory(modules)
add_subdirectory(tests)
