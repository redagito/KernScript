# Umbrella for the sample host applications. Only dispatches, it defines nothing itself.
#
#   HelloKern/ minimal host: compiles a script, runs it, prints the result
# More sample apps follow.
include (${CMAKE_CURRENT_LIST_DIR}/SampleSupport.cmake)
if (KERNSCRIPT_BUILD_SAMPLES)
    add_subdirectory (HelloKern)
    add_subdirectory (CalcPad)
    add_subdirectory (TextFlow)
    add_subdirectory (PolicyLab)
    # Native Win32 sample: Windows only. Cross-platform UI samples live in
    # their own folders without a system guard.
    if (WIN32)
            add_subdirectory (NativeEvents)
    endif ()
endif ()
if (KERNSCRIPT_BUILD_GUI_SAMPLES)
    # Cross-platform twin of NativeEvents above, built on FLTK. No system guard:
    # FLTK abstracts the native toolkit, so this builds on every platform.
    add_subdirectory (FltkEvents)
    # Scripted Game of Life on FLTK. No system guard: cross-platform like FltkEvents.
    add_subdirectory (LifeLab)
    # Script-filtered task board on FLTK. No system guard: cross-platform like LifeLab.
    add_subdirectory (Daybook)
    # Formula grid on FLTK. No system guard: cross-platform like Daybook.
    add_subdirectory (Gridbook)
    # Stateful conversation authoring on FLTK. No system guard: cross-platform like Gridbook.
    add_subdirectory (Dialogue)
    # Script-built UI on FLTK. No system guard: cross-platform like Dialogue.
    add_subdirectory (Toolsmith)
    # Scriptable paint studio on FLTK. No system guard: cross-platform like Toolsmith.
    add_subdirectory (Canvas)
    # Script-driven visual novel on FLTK. No system guard: cross-platform like Canvas.
    add_subdirectory (Storybook)
    # Algorithmic music workstation on FLTK. No system guard: cross-platform like Storybook.
    add_subdirectory (Cadence)
    # Scripted instruments dashboard on FLTK. No system guard: cross-platform like Cadence.
    add_subdirectory (SignalDesk)
endif ()

# These draw only the application's widget hierarchy into an offscreen image.
# They do not show native windows or capture the desktop. Other platforms keep
# the model verifiers independent of a running display server.
if (BUILD_TESTING AND WIN32)
    set (sample_preview_directory "${CMAKE_BINARY_DIR}/sample-previews")
    file (MAKE_DIRECTORY "${sample_preview_directory}")
    foreach (sample IN ITEMS Canvas Gridbook Cadence Daybook Storybook LifeLab SignalDesk Toolsmith Dialogue)
        if (TARGET ${sample})
            add_test (NAME ${sample}.Layout COMMAND $<TARGET_FILE:${sample}>
                --preview "${sample_preview_directory}/${sample}.png")
            set_tests_properties (${sample}.Layout PROPERTIES
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "samples;layout")
        endif ()
    endforeach ()
    if (TARGET Toolsmith)
        add_test (NAME Toolsmith.NotesLayout COMMAND $<TARGET_FILE:Toolsmith>
            --script "${CMAKE_CURRENT_SOURCE_DIR}/Toolsmith/scripts/notes.ks"
            --preview "${sample_preview_directory}/ToolsmithNotes.png")
        set_tests_properties (Toolsmith.NotesLayout PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" LABELS "samples;layout")
    endif ()
endif ()
