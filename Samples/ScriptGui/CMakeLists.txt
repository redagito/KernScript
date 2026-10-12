add_executable (ScriptGui source/main.cpp)

target_link_libraries (ScriptGui
	PRIVATE KernScriptLib
	PRIVATE user32
)
