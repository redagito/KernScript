# Algorithmic music workstation on FLTK. No system guard: cross-platform
# like Storybook. Composition scripts live as separate module files.
find_package (fltk REQUIRED)

add_executable (Cadence source/main.cpp)

target_link_libraries (Cadence
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The music modules live as separate script files loaded from disk at
# startup and on script reload. Copy them beside the exe for both the GUI
# and the headless verifier; the host also falls back to CWD-relative paths.
kern_sample_copy_directory (Cadence music)

if (BUILD_TESTING)
	add_test (NAME Cadence.Verify COMMAND $<TARGET_FILE:Cadence> --verify
		--test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Cadence")
	set_tests_properties (Cadence.Verify PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif ()
