# Keep runtime modules current even when only a script changed and the
# executable did not relink. Copying only after link leaves stale scripts.
function (kern_sample_copy_directory target directory)
    add_custom_target (${target}Assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}"
            "$<TARGET_FILE_DIR:${target}>/${directory}"
        COMMENT "Updating ${target} runtime assets"
        VERBATIM
    )
    add_dependencies (${target} ${target}Assets)
endfunction ()
