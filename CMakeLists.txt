cmake_minimum_required (VERSION 4.4.3)

project (KernScript LANGUAGES CXX)

# Build settings that apply to every project below. Everything project
# specific lives in the project's own CMakeLists.txt.
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Required once at the top level so that projects added below can register
# their tests with add_test.
enable_testing ()

add_subdirectory (Lib)
add_subdirectory (Test)
add_subdirectory (Samples)
add_subdirectory (Benchmarks)
