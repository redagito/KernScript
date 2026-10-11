# Umbrella for the test projects. Only dispatches, it defines nothing itself.
#
#   TestSupport/     shared helper library
#   UnitTest/        single production type, no collaborators
#   IntegrationTest/ several real components wired together
#   EndToEndTest/    the complete host workflow through the public API

add_subdirectory (TestSupport)
add_subdirectory (UnitTest)
add_subdirectory (IntegrationTest)
add_subdirectory (EndToEndTest)
