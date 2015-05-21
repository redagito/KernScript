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