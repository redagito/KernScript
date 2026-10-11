# Catch2 is not re-exported by the Conan package's catch_discover_tests
# module, so tests are registered per executable. Tag the cases with
# "[unit]" to filter a subset.

file (GLOB_RECURSE UNIT_TEST_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

add_executable (KernScriptTestUnit ${UNIT_TEST_SOURCES})

find_package (Catch2 REQUIRED)

target_link_libraries (KernScriptTestUnit PRIVATE
	KernScriptTestSupport
	# Private implementation headers, needed to unit test the classes there.
	KernScriptLibInternal
	Catch2::Catch2WithMain
)

add_test (NAME KernScriptTestUnit COMMAND KernScriptTestUnit)
