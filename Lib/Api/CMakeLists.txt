file (GLOB API_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

find_package (fmt REQUIRED)

# The product library: public API plus factories. Links the implementation
# privately: Kern.cpp constructs implementation classes, but consumers must
# never see their headers.
add_library (KernScriptLib ${API_SOURCES})

target_link_libraries (KernScriptLib
	PUBLIC KernScriptApiHeaders
	PUBLIC fmt::fmt
	PRIVATE KernScriptLibImpl
)
