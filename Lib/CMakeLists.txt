file (GLOB_RECURSE KERNSCRIPT_LIB_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

add_library (KernScriptLib ${KERNSCRIPT_LIB_SOURCES})

target_include_directories (KernScriptLib
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# The implementation headers are private to the library. They are exposed
# through an INTERFACE target so the unit tests can exercise the implementation
# classes without hardcoding a path into this project. Nothing installs or
# exports either target, so the headers stay internal to the build.
add_library (KernScriptLibInternal INTERFACE)
target_include_directories (KernScriptLibInternal INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries (KernScriptLib PUBLIC KernScriptLibInternal)
