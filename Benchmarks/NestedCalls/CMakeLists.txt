add_executable (NestedCalls source/main.cpp)

target_link_libraries (NestedCalls
	PRIVATE KernScriptLib
)

# The --profile dumps below need the concrete VM type: same tree, private include.
target_include_directories (NestedCalls
	PRIVATE ${CMAKE_SOURCE_DIR}/Lib/Impl/include
)
