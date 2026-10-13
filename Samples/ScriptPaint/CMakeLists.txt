# Scriptable paint studio: FLTK canvas, host-owned pixels, script-owned tools.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptPaint source/main.cpp)

target_link_libraries (ScriptPaint
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The studio scripts live as separate files loaded from disk on every button
# press (hot-reload by design). Copy them beside the exe so --verify works
# from the build tree; the host also falls back to CWD-relative candidates.
add_custom_command (TARGET ScriptPaint POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:ScriptPaint>/scripts
)
