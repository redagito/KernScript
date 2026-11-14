# Script-driven visual novel: FLTK stage, host-owned flags, chapters as
# script modules with a diamond import tree (chapter > cast + world,
# world > cast).
#
# NOTE (Linux): the FLTK Conan package needs the X11 development packages on
# the build machine, e.g. libx11-dev, libxext-dev, libxft-dev and libxinerama-dev.
# Windows and macOS need nothing extra.
find_package (fltk REQUIRED)

add_executable (Storybook source/main.cpp)

target_link_libraries (Storybook
	PRIVATE KernScriptLib
	PRIVATE fltk::fltk
)

# The chapters live as separate script files loaded from disk at startup,
# on every chapter switch and on every reload. Copy them beside the exe so
# --verify works from the build tree; the host also falls back to
# CWD-relative candidates.
kern_sample_copy_directory (Storybook chapters)

if (BUILD_TESTING)
    add_test (NAME Storybook.Verify COMMAND $<TARGET_FILE:Storybook> --verify --test-root "${CMAKE_SOURCE_DIR}/build/test-tmp/Storybook")
endif ()
