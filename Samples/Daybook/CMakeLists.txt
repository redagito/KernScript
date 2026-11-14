# The verifier invokes this same executable with --verify. That path never
# constructs or shows an FLTK widget.
find_package (fltk REQUIRED)
add_executable (Daybook source/main.cpp)
target_link_libraries (Daybook PRIVATE KernScriptLib fltk::fltk)
kern_sample_copy_directory (Daybook policies)

if (BUILD_TESTING)
	add_test (NAME Daybook.Verify COMMAND $<TARGET_FILE:Daybook> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Daybook")
endif ()
