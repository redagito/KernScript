# Script-built UI on FLTK: the host is a bare shell, the script builds widgets.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptUi source/main.cpp)

target_link_libraries (ScriptUi
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The UI scripts live as separate files loaded from disk at startup.
# Copy them beside the exe so --verify works from the build tree; the host
# also falls back to CWD-relative candidates.
add_custom_command (TARGET ScriptUi POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:ScriptUi>/scripts
)
