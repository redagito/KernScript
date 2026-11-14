add_executable (NativeEvents source/main.cpp)

target_link_libraries (NativeEvents
	PRIVATE KernScriptLib
	PRIVATE user32
)
