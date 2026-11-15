add_executable (PerformanceLab source/main.cpp)
target_link_libraries (PerformanceLab PRIVATE KernScriptLib)

add_executable (PerformanceMemory source/main.cpp source/Allocations.cpp)
target_compile_definitions (PerformanceMemory PRIVATE KERN_BENCH_MEMORY)
target_link_libraries (PerformanceMemory PRIVATE KernScriptLib)
