# Algorithmic music workstation on FLTK. No system guard: cross-platform
# like ScriptNovel. Composition scripts live as separate module files.
find_package (fltk REQUIRED)

add_executable (ScriptMusic source/main.cpp)

target_link_libraries (ScriptMusic
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The music modules live as separate script files loaded from disk at
# startup and on every preset switch. Copy them beside the exe so
# --verify works from the build tree; the host also falls back to
# CWD-relative candidates.
add_custom_command (TARGET ScriptMusic POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/music $<TARGET_FILE_DIR:ScriptMusic>/music
)
