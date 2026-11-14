add_executable (PolicyLab source/main.cpp)

target_link_libraries (PolicyLab
	PRIVATE KernScriptLib
)

kern_sample_copy_directory (PolicyLab policies)

if (BUILD_TESTING)
    add_test (NAME PolicyLab.Verify COMMAND $<TARGET_FILE:PolicyLab> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/PolicyLab")
endif ()
