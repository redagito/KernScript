# Scripted instruments dashboard on FLTK. No system guard:
# cross-platform like ScriptMusic. The dash scripts live as separate
# module files (stats hub, alerts + views leaves) loaded from disk.
find_package (fltk REQUIRED)

add_executable (ScriptDash source/main.cpp)

target_link_libraries (ScriptDash
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The dash modules live as separate script files loaded from disk at
# startup and on every reload. Copy them beside the exe so --verify works
# from the build tree; the host also falls back to CWD-relative
# candidates. The tailed feed file is seeded by the host at startup when
# missing and is NOT copied (build tree only, never the source tree).
add_custom_command (TARGET ScriptDash POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/dash $<TARGET_FILE_DIR:ScriptDash>/dash
)
