# Scripted greenhouse dashboard on FLTK. Policy modules are loaded from disk.
find_package (fltk REQUIRED)

add_executable (SignalDesk source/main.cpp)

target_link_libraries (SignalDesk
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
target_include_directories (SignalDesk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Support)

# Copy policy modules and the recorded greenhouse replay beside the executable.
kern_sample_copy_directory (SignalDesk dash)

if (BUILD_TESTING)
    add_test (NAME SignalDesk.Verify COMMAND $<TARGET_FILE:SignalDesk> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/SignalDesk")
endif ()
