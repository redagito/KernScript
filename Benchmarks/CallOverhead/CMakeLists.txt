add_executable (CallOverhead source/main.cpp)

target_link_libraries (CallOverhead
	PRIVATE KernScriptLib
)
