# Script-filtered task board: FLTK list, host-owned tasks, script-owned policy.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxfixes-dev,
# libxft-dev and libxinerama-dev. Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptBoard source/main.cpp)

target_link_libraries (ScriptBoard
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)
