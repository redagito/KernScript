cmake_minimum_required (VERSION 4.4.3)

project (KernScript VERSION 1.0.0 LANGUAGES CXX)

# Build settings that apply to every project below. Everything project
# specific lives in the project's own CMakeLists.txt.
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Required once at the top level so that projects added below can register
# their tests with add_test.
include (CTest)
option (KERNSCRIPT_BUILD_SAMPLES "Build command-line and native host samples" OFF)
option (KERNSCRIPT_BUILD_GUI_SAMPLES "Build the FLTK samples (requires FLTK)" OFF)
option (KERNSCRIPT_BUILD_BENCHMARKS "Build benchmark executables" OFF)

add_subdirectory (Lib)
if (BUILD_TESTING)
    add_subdirectory (Test)
endif ()
if (KERNSCRIPT_BUILD_SAMPLES OR KERNSCRIPT_BUILD_GUI_SAMPLES)
    add_subdirectory (Samples)
endif ()
if (KERNSCRIPT_BUILD_BENCHMARKS)
    add_subdirectory (Benchmarks)
endif ()
