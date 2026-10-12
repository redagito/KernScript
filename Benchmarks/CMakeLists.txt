# Umbrella for the benchmark hosts. Only dispatches, it defines nothing itself.
#
#   common/          shared header-only harness, no executable
#   CallOverhead/    empty callFunction cost
#   StackOps/        script loop pushing/popping values
#   ArithLoop/       script-side while accumulation
#   ExternRoundtrip/ valueless plus valued extern call cost
#   NestedCalls/     recursive fib plus deep call chain
#   Pipeline/        compile plus assemble plus load time
#   RealProgram/     scripted computation mix, the anti-microbenchmark anchor
add_subdirectory (CallOverhead)
add_subdirectory (StackOps)
add_subdirectory (ArithLoop)
add_subdirectory (ExternRoundtrip)
add_subdirectory (NestedCalls)
add_subdirectory (Pipeline)
add_subdirectory (RealProgram)
