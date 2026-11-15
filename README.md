# KernScript

KernScript embeds a bytecode scripting language in C++ applications. The host
registers C++ functions, compiles scripts, loads verified bytecode, and calls
script entry points. Scripts exchange data through those host functions.

Values are signed/unsigned 32-bit integers, 32-bit floats, and strings. Scripts
support functions, conditions, loops, and modules. Each call runs to completion
within configurable instruction, stack, and memory budgets.

## Build

Requires CMake 4.4.3+, Conan 2, and a C++23 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

Optional Conan settings:

| Option | Enables |
| --- | --- |
| `-o "&:with_samples=True"` | Command-line samples and the Windows event tutorial |
| `-o "&:with_gui_samples=True"` | FLTK samples and their dependency |
| `-o "&:with_benchmarks=True"` | Performance workloads |
| `-o "&:with_tests=False"` | A build without tests or Catch2 |

Use the same configuration for Conan, building, and testing. Substitute
`Release` for `Debug` when measuring performance. Conan forwards these settings
to CMake; direct configuration uses `BUILD_TESTING`, `KERNSCRIPT_BUILD_SAMPLES`,
`KERNSCRIPT_BUILD_GUI_SAMPLES`, and `KERNSCRIPT_BUILD_BENCHMARKS`.

## Usage

Start with [Getting Started](docs/Getting%20Started.md). The
[Host Manual](docs/Host%20Manual.md) covers embedding, ownership, and budgets;
the [Diagnostics Guide](docs/Diagnostics%20Guide.md) explains failures.

For scripts, use the [language reference](docs/Scripting%20Language.md) and
[cookbook](docs/Scripting%20Cookbook.md). The
[assembly reference](docs/Assembly%20Language.md) describes the lower-level format.

See [samples](Samples/README.md) for applications and extension examples, or
[benchmarks](Benchmarks/README.md) for performance measurement.

## License

[MIT](LICENSE).
