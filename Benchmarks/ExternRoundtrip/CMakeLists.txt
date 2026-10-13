add_executable (ExternRoundtrip source/main.cpp)

target_link_libraries (ExternRoundtrip
	PRIVATE KernScriptLib
)

# The --profile dumps below need the concrete VM type: same tree, private include.
target_include_directories (ExternRoundtrip
	PRIVATE ${CMAKE_SOURCE_DIR}/Lib/Impl/include
)
