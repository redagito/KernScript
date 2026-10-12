# Eliza-style scripted therapist: FLTK chat log, host-owned mood, script-owned brain.
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptShrink source/main.cpp)

target_link_libraries (ScriptShrink
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The brains live as separate script files loaded from disk at startup and on
# every personality switch. Copy them beside the exe so --verify works from
# the build tree; the host also falls back to CWD-relative candidates.
add_custom_command (TARGET ScriptShrink POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:ScriptShrink>/scripts
)
