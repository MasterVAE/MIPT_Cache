add_library(cache_lib INTERFACE)
target_include_directories(cache_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(cache_exec 
    src/main.cpp
    src/config.cpp
)

target_link_libraries(cache_exec 
    PRIVATE 
        cache_lib
        cache_compile_flags
        cache_strict_debug
)

add_test(
    NAME cache_run_with_config
    COMMAND sh -c "printf '1\\n10 LFU\\n' > tmp_test.cfg && echo '0' | $<TARGET_FILE:cache_exec> tmp_test.cfg"
)