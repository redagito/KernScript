# Script-driven visual novel: FLTK stage, host-owned flags, chapters as
# script modules with a diamond import tree (chapter > cast + world,
# world > cast).
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (ScriptNovel source/main.cpp)

target_link_libraries (ScriptNovel
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The chapters live as separate script files loaded from disk at startup,
# on every chapter switch and on every reload. Copy them beside the exe so
# --verify works from the build tree; the host also falls back to
# CWD-relative candidates.
add_custom_command (TARGET ScriptNovel POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/chapters $<TARGET_FILE_DIR:ScriptNovel>/chapters
)
