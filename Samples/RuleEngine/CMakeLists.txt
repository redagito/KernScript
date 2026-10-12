add_executable (RuleEngine source/main.cpp)

target_link_libraries (RuleEngine
	PRIVATE KernScriptLib
)
