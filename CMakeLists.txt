cmake_minimum_required (VERSION 3.1)
project (KernScript)

set (CXX_STANDARD_REQUIRED ON)

set (KERNSCRIPT_LIB_NAME ${PROJECT_NAME}Lib)
add_subdirectory (Lib)

enable_testing ()
set (KERNSCRIPT_TEST_NAME ${PROJECT_NAME}Test)
add_subdirectory (Test)