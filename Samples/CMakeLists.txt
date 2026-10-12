# Umbrella for the sample host applications. Only dispatches, it defines nothing itself.
#
#   HelloScript/ minimal host: compiles a script, runs it, prints the result
# More sample apps follow.
add_subdirectory (HelloScript)
add_subdirectory (Calc)
add_subdirectory (TextPipe)
add_subdirectory (RuleEngine)
# Native Win32 sample: Windows only. Cross-platform UI samples live in
# their own folders without a system guard.
if (WIN32)
    add_subdirectory (ScriptGui)
endif ()
# Cross-platform twin of ScriptGui above, built on FLTK. No system guard:
# FLTK abstracts the native toolkit, so this builds on every platform.
add_subdirectory (ScriptFltk)
# Scripted Game of Life on FLTK. No system guard: cross-platform like ScriptFltk.
add_subdirectory (ScriptLife)
# Script-filtered task board on FLTK. No system guard: cross-platform like ScriptLife.
add_subdirectory (ScriptBoard)
# Formula grid on FLTK. No system guard: cross-platform like ScriptBoard.
add_subdirectory (ScriptSheet)
