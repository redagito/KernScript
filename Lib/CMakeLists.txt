file (GLOB_RECURSE SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.hxx
)

add_library (${KERNSCRIPT_LIB_NAME} ${SOURCE_FILES})

# Implementation headers are private to the library. They are exposed through
# an INTERFACE target so the unit and integration tests can exercise the
# implementation classes directly. Nothing installs or exports either target,
# so these headers stay internal to the build.
add_library (${KERNSCRIPT_LIB_NAME}Internal INTERFACE)
target_include_directories (${KERNSCRIPT_LIB_NAME}Internal INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries (${KERNSCRIPT_LIB_NAME}
	PUBLIC ${KERNSCRIPT_LIB_NAME}Internal
)

target_include_directories (${KERNSCRIPT_LIB_NAME}
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
