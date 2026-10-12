# Formula grid: FLTK sheet, host-owned cells, script-owned formulas.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxfixes-dev,
# libxft-dev and libxinerama-dev. Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptSheet source/main.cpp)

target_link_libraries (ScriptSheet
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
