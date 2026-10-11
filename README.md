# KernScript

A bytecode interpreted scripting language for C++. Educational project, not fit
for productive use.

Scripts travel through four stages: the compiler produces assembly-like code
from script source, the assembler turns that into byte code files in a custom
format, and the virtual machine executes them and calls into host functions.

The compiler is still a stub, so working scripts are currently written by hand
in the assembly dialect kept under `Test/EndToEndTest/data/asm/`.

## Building

Requires CMake 4.4 or newer, Conan 2, and a C++17 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

The build type given to Conan must match the configuration given to CMake.
Substitute `Release` for `Debug` throughout to build the release configuration.

## License

MIT, see `LICENSE`.
