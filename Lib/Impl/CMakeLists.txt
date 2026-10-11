file (GLOB_RECURSE IMPL_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx
)

find_package (fmt REQUIRED)
find_package (spdlog REQUIRED)

# Implementation detail, not for users. Exposes its source tree so the unit
# and integration tests can exercise the implementation classes directly;
# nothing installs or exports these targets.
add_library (KernScriptLibImpl ${IMPL_SOURCES})

target_include_directories (KernScriptLibImpl
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries (KernScriptLibImpl
	PUBLIC KernScriptApiHeaders
	PUBLIC fmt::fmt
	PRIVATE spdlog::spdlog
)
