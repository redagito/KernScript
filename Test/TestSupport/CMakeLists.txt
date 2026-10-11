file (GLOB_RECURSE KERNSCRIPT_TEST_SUPPORT_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

add_library (KernScriptTestSupport STATIC ${KERNSCRIPT_TEST_SUPPORT_SOURCES})

# Public header of the helper library. Exported through the target so the test
# projects include "testutil/TestSupport.hpp" without naming this directory.
target_include_directories (KernScriptTestSupport
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries (KernScriptTestSupport
	PUBLIC KernScriptLib
)
