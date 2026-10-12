# Scripted Game of Life: FLTK grid, host-owned state, script-owned rules.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxfixes-dev,
# libxft-dev and libxinerama-dev. Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptLife source/main.cpp)

target_link_libraries (ScriptLife
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
