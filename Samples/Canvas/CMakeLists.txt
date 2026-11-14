# Scriptable paint studio: FLTK canvas, host-owned pixels, script-owned tools.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (Canvas source/main.cpp)

target_link_libraries (Canvas
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The studio scripts live as separate files loaded from disk on every button
# press (hot-reload by design). Copy them beside the exe so --verify works
# from the build tree; the host also falls back to CWD-relative candidates.
kern_sample_copy_directory (Canvas scripts)

if (BUILD_TESTING)
    add_test (NAME CanvasHeadlessVerifier COMMAND $<TARGET_FILE:Canvas>
        --verify-headless ${CMAKE_CURRENT_BINARY_DIR}/verify)
    set_tests_properties (CanvasHeadlessVerifier PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif ()
