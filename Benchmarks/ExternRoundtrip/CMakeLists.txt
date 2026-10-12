add_executable (ExternRoundtrip source/main.cpp)

target_link_libraries (ExternRoundtrip
	PRIVATE KernScriptLib
)
