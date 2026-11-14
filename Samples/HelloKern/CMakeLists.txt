add_executable (HelloKern source/main.cpp)

target_link_libraries (HelloKern
	PRIVATE KernScriptLib
)
