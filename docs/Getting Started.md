# Getting Started with KernScript

15-minute path: build, run, script, extend,
diagnose.

## 1. Build (5 min)

Needs CMake 4.4+, Conan 2, a C++23 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
```

## 2. Run the HelloScript sample (2 min)

```sh
build/Samples/HelloScript/Debug/HelloScript.exe
```

Prints:

```text
hello from script
42
```

Ran: exit 0.

## 3. Write and run a first script (4 min)

hello.ks:

```ks
extern println(x);
func main() {
  println("hi");
  println(40 + 2);
}
```

Compile, assemble, load and call "main" exactly
like Samples/HelloScript/source/main.cpp:27-60
(compiler, assembler, VM, `addFunction` for
`println`, `load`, `callFunction`). Prints
"hi" and 42. That sample source is this script.

## 4. Add one extern (3 min)

```cpp
int double_it(int x) { return 2 * x; }
vm->addFunction("double_it",
  kern::createExternFunction(&double_it));
```

```ks
extern double_it(x);
extern emit(x);
func main() {
  emit(double_it(21));
}
```

Emits "42". Ran end-to-end.

## 5. Read one real error (1 min)

Drop the extern declaration:

```ks
func main() {
  println("hi");
}
```

Compile fails and logs exactly this:

```text
hello.ks:2:3: error: Call to undeclared function.
note: declare it with extern or func
```

Ran: byte-for-byte output above. `compile`
returns false; fix by declaring the extern.

Prove the tree is green:

```sh
ctest --test-dir build -C Debug
```

Verified: ran the sample, the extern snippet
and the error run above.
