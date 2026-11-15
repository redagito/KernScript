# Getting Started with KernScript

Build the library, run a sample, register a host function, and diagnose an error.

## 1. Build

Needs CMake 4.4.3+, Conan 2, a C++23 compiler.

```sh
conan install . -s build_type=Debug --build=missing -o "&:with_samples=True"
cmake --preset conan-default
cmake --build build --config Debug
```

## 2. Run the HelloKern sample

For a Visual Studio build:

```sh
build/Samples/HelloKern/Debug/HelloKern.exe
```

Other generators generally omit `Debug`; non-Windows executables omit `.exe`.

Prints:

```text
hello from script
42
```

## 3. Write and run a first script

hello.ks:

```ks
extern println(x) -> void;
func main() -> void {
  println("hi");
  println(40 + 2);
}
```

Compile, assemble, load, and call `main` using the embedding pattern in
[HelloKern](../Samples/HelloKern/source/main.cpp). Register `println` with
`addFunction` before executing the script. This script prints "hi" and 42.

## 4. Add one extern

```cpp
int double_it(int x) { return 2 * x; }
void emit(int value) { std::cout << value << '\n'; }
vm->addFunction("double_it", &double_it);
vm->addFunction("emit", &emit);
```

```ks
extern double_it(x) -> value;
extern emit(x) -> void;
func main() -> void {
  emit(double_it(21));
}
```

The host's `emit` function receives 42.

## 5. Read one error

Drop the extern declaration:

```ks
func main() -> void {
  println("hi");
}
```

Compilation fails. With `hello.ks` passed as the source name, diagnostics include:

```text
hello.ks:2:3: error: Call to undeclared function.
note: declare it with extern or func
```

`compile` returns false; fix the error by declaring the extern.

Run the test suite:

```sh
ctest --test-dir build -C Debug
```
