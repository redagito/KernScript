# Scripted Game of Life: FLTK grid, host-owned state, script-owned rules.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxfixes-dev,
# libxft-dev and libxinerama-dev. Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (LifeLab source/main.cpp)
target_include_directories (LifeLab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Support)

target_link_libraries (LifeLab
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
kern_sample_copy_directory (LifeLab rules)

if (BUILD_TESTING)
	add_test (NAME LifeLab.Verify COMMAND $<TARGET_FILE:LifeLab> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/LifeLab")
endif ()
