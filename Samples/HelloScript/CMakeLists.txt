add_executable (HelloScript source/main.cpp)

target_link_libraries (HelloScript
	PRIVATE KernScriptLib
)
