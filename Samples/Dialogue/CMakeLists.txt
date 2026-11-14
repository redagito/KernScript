# Stateful conversation authoring: FLTK transcript, host-owned state and script-owned rules.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (Dialogue source/main.cpp)

target_link_libraries (Dialogue
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The brains live as separate script files loaded from disk at startup and on
# every personality switch. Copy them beside the exe so --verify works from
# the build tree; the host also falls back to CWD-relative candidates.
kern_sample_copy_directory (Dialogue scripts)

if (BUILD_TESTING)
    add_test (NAME Dialogue.Verify COMMAND $<TARGET_FILE:Dialogue> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Dialogue")
endif ()
