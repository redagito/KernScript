add_executable (TextFlow source/main.cpp)

target_link_libraries (TextFlow
	PRIVATE KernScriptLib
)

add_custom_target (TextFlowAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TextFlow>"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/rules.ks"
		"${CMAKE_CURRENT_SOURCE_DIR}/sample.log"
		"$<TARGET_FILE_DIR:TextFlow>"
    VERBATIM
)

add_dependencies (TextFlow TextFlowAssets)

if (BUILD_TESTING)
	add_test (NAME TextFlowVerifier
		COMMAND $<TARGET_FILE:TextFlow> --verify
	)
	set_tests_properties (TextFlowVerifier PROPERTIES
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	)
endif ()
