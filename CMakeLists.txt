# Minimum cmake version
cmake_minimum_required(VERSION 2.8)

# Project name
project(KernScript)

# Include
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/source/
	${CMAKE_CURRENT_SOURCE_DIR}/include/
)

# Sources
file(GLOB_RECURSE SRC_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

# Add third party sources
set(SRC_FILES ${SRC_FILES} ${THIRD_PARTY_SOURCES})

# Flags
if (UNIX OR MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif (UNIX OR MINGW)

# Add library
add_library(kernscript ${SRC_FILES})


# Test sources
file(GLOB_RECURSE TEST_SRC_FILES
	${CMAKE_SOURCE_DIR}/test/*.cpp
)

# Test exectuable
add_executable(kerntest ${TEST_SRC_FILES})

# Link script library
target_link_libraries(kerntest
	kernscript
)