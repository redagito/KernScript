# Formula grid: FLTK sheet, host-owned cells, script-owned formulas.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxfixes-dev,
# libxft-dev and libxinerama-dev. Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (Gridbook source/main.cpp)

target_link_libraries (Gridbook
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
kern_sample_copy_directory (Gridbook scripts)
if (BUILD_TESTING)
    add_test (NAME Gridbook.Verify COMMAND $<TARGET_FILE:Gridbook> --verify
        --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Gridbook")
endif ()
