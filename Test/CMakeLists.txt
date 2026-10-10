# Test is an umbrella. It owns no sources itself and only wires up the test
# projects in its subdirectories. Each subdirectory is a project holding its
# own source folder:
#
#   TestSupport/    helper library the test levels link against
#   UnitTest/       single production type, no collaborators
#   IntegrationTest/ several real components wired together
#   EndToEndTest/   the complete host workflow through the public API
#
# Every test executable gets its own CTest entry. Test cases are additionally
# tagged by level so subsets can be selected with Catch2 tag filters.
#
# The Catch2 package distributed by Conan does not export the
# catch_discover_tests() module, so tests are registered per executable rather
# than per test case. Use "[unit]", "[integration]" and "[e2e]" tags to filter.

find_package (Catch2 REQUIRED)

set (KERNSCRIPT_TEST_LEVELS UnitTest IntegrationTest EndToEndTest)

# Shared test helper library. This is a private build artefact, so it is not
# given the same visibility treatment as the production library headers.
add_library (${KERNSCRIPT_TEST_NAME}Support STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/TestSupport/source/TestSupport.cpp
)

target_include_directories (${KERNSCRIPT_TEST_NAME}Support PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/TestSupport/source
)

target_link_libraries (${KERNSCRIPT_TEST_NAME}Support PUBLIC ${KERNSCRIPT_LIB_NAME})

foreach (LEVEL ${KERNSCRIPT_TEST_LEVELS})
	# Target names are derived from the directory name minus the "Test" suffix.
	string (REGEX REPLACE "Test$" "" LEVEL_SUFFIX ${LEVEL})
	set (TARGET_NAME ${KERNSCRIPT_TEST_NAME}${LEVEL_SUFFIX})

	file (GLOB_RECURSE LEVEL_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/${LEVEL}/source/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/${LEVEL}/source/*.cxx
	)

	add_executable (${TARGET_NAME} ${LEVEL_SOURCES})

	target_include_directories (${TARGET_NAME} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/${LEVEL}/source
	)

	# The INTERFACE library carries the implementation headers, so the unit
	# tests can include them directly.
	target_link_libraries (${TARGET_NAME} PRIVATE
		${KERNSCRIPT_LIB_NAME}Internal
		${KERNSCRIPT_TEST_NAME}Support
		Catch2::Catch2WithMain
	)

	# Test fixtures are looked up relative to the working directory and byte
	# code output goes next to them, so both directories are created beside the
	# executable. CTest is directed to that same directory via add_test, which
	# keeps manual runs and CTest runs in agreement on multi configuration
	# generators.
	add_custom_command (TARGET ${TARGET_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/data
			$<TARGET_FILE_DIR:${TARGET_NAME}>/data
		COMMAND ${CMAKE_COMMAND} -E make_directory
			$<TARGET_FILE_DIR:${TARGET_NAME}>/asm_out
	)

	add_test (NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})

	# CMake 3.20 and later evaluate generator expressions here.
	set_tests_properties (${TARGET_NAME} PROPERTIES
		WORKING_DIRECTORY $<TARGET_FILE_DIR:${TARGET_NAME}>
		LABELS ${LEVEL_SUFFIX}
	)
endforeach ()
