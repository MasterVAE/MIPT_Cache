include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests 
    unit/unit_cache_test.cpp
    unit/test_runner.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        cache_lib
        cache_compile_flags
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME e2e
    COMMAND Python3::Interpreter test_program.py --binary $<TARGET_FILE:cache_exec>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/e2e
)