# Script-built UI on FLTK: the host is a bare shell, the script builds widgets.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (Toolsmith source/main.cpp)

target_link_libraries (Toolsmith
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The UI scripts live as separate files loaded from disk at startup.
# Copy them beside the exe so --verify works from the build tree; the host
# also falls back to CWD-relative candidates.
kern_sample_copy_directory (Toolsmith scripts)

if (BUILD_TESTING)
	add_test (NAME Toolsmith.Verify COMMAND $<TARGET_FILE:Toolsmith> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Toolsmith")
endif ()
