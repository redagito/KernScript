# Catch2 is not re-exported by the Conan package's catch_discover_tests
# module, so tests are registered per executable. Tag the cases with
# "[e2e]" to filter a subset.

file (GLOB_RECURSE END_TO_END_TEST_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

add_executable (KernScriptTestEndToEnd ${END_TO_END_TEST_SOURCES})

find_package (Catch2 REQUIRED)

target_link_libraries (KernScriptTestEndToEnd PRIVATE
	KernScriptTestSupport
	Catch2::Catch2WithMain
)

# Test fixtures are owned by this project and looked up relative to the
# working directory, so they are copied next to the executable. Byte code
# output goes into a directory created beside them.
add_custom_command (TARGET KernScriptTestEndToEnd POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/data
		$<TARGET_FILE_DIR:KernScriptTestEndToEnd>/data
	COMMAND ${CMAKE_COMMAND} -E make_directory
		$<TARGET_FILE_DIR:KernScriptTestEndToEnd>/asm_out
)

add_test (NAME KernScriptTestEndToEnd COMMAND KernScriptTestEndToEnd)

# CMake 3.20 and later evaluate generator expressions here. Pointing CTest at
# the executable's directory keeps manual runs and CTest runs in agreement on
# multi configuration generators.
set_tests_properties (KernScriptTestEndToEnd PROPERTIES
	WORKING_DIRECTORY $<TARGET_FILE_DIR:KernScriptTestEndToEnd>
	LABELS e2e
)
