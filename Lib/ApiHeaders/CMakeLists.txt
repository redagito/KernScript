# Public interface headers, owned by this project. A leaf target so both the
# product library and the implementation can consume the headers without a
# dependency cycle between the two libraries.
add_library (KernScriptApiHeaders INTERFACE)
target_include_directories (KernScriptApiHeaders INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
