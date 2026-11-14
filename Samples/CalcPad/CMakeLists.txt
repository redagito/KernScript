add_executable (CalcPad source/main.cpp)
target_link_libraries (CalcPad PRIVATE KernScriptLib)

kern_sample_copy_directory (CalcPad scripts)

if (BUILD_TESTING)
	add_test (NAME CalcPad.Verify COMMAND $<TARGET_FILE:CalcPad> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/CalcPad")
endif ()
