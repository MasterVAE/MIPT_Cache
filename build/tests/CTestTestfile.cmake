# CMake generated Testfile for 
# Source directory: /home/kramer/repos/MIPT_Cache/tests
# Build directory: /home/kramer/repos/MIPT_Cache/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/home/kramer/repos/MIPT_Cache/build/tests/unit_tests[1]_include.cmake")
add_test([=[e2e]=] "/usr/bin/python3.13" "test_program.py" "--binary" "/home/kramer/repos/MIPT_Cache/build/modules/cache_exec")
set_tests_properties([=[e2e]=] PROPERTIES  WORKING_DIRECTORY "/home/kramer/repos/MIPT_Cache/tests/e2e" _BACKTRACE_TRIPLES "/home/kramer/repos/MIPT_Cache/tests/CMakeLists.txt;25;add_test;/home/kramer/repos/MIPT_Cache/tests/CMakeLists.txt;0;")
subdirs("../_deps/googletest-build")
