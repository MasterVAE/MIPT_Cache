add_library(cache_lib INTERFACE)

target_include_directories(cache_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(cache_exec 
    src/main.cpp
    src/config.cpp
)

target_link_libraries(cache_exec PRIVATE cache_lib)
target_compile_options(cache_exec PRIVATE
    $<$<CONFIG:DEBUG>:-Wall;-Wpedantic;-DDEBUG;-O0;-g;-Wmissing-declarations;-Wcast-align;-Wshadow;-Wsign-conversion>
    $<$<CONFIG:RELEASE>:-O3>
)

if (ENABLE_ASAN)
    target_compile_options(cache_exec PRIVATE
        $<$<CONFIG:DEBUG>: -fsanitize=address -fno-omit-frame-pointer>
    )
    target_link_options(cache_exec PRIVATE
        $<$<CONFIG:DEBUG>: -fsanitize=address>
    )
endif()

option(ENABLE_UNIT_TESTS OFF)

if(ENABLE_UNIT_TESTS)
    target_compile_definitions(cache_exec PUBLIC ENABLE_UNIT_TESTS=1)
endif()

add_test(
    NAME cache_run_with_config
    COMMAND cache_exec ${CMAKE_CURRENT_SOURCE_DIR}/config.cfg
)



