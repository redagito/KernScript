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
