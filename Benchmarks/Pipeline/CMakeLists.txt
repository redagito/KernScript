add_executable (Pipeline source/main.cpp)

target_link_libraries (Pipeline
	PRIVATE KernScriptLib
)
