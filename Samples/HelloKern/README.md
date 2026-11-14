# HelloKern

HelloKern is the minimal embedding tutorial. Enable command-line samples and
build the `HelloKern` target, then run it. It prints:

```text
hello from script
42
```

The host compiles the embedded source to assembly, assembles bytecode, binds
the C++ `println(std::string)` function, loads the program, and calls the
zero-argument `main() -> void` script entry. The script controls what to print
and performs the arithmetic; C++ supplies output and owns the VM lifetime.

Change `40 + 2` in `source/main.cpp` to another expression and rebuild for a
first experiment. Register another typed C++ function to add an application
capability. The extern declaration in the script states its parameter count
and return kind, while the typed binding checks conversion of each argument.

The script is deliberately embedded to keep the complete workflow in one short
file. [TextFlow](../TextFlow/README.md) demonstrates a practical command-line
app with external rules; [Canvas](../Canvas/README.md) demonstrates reloadable
modules in an interactive application. Use those projects for larger real
workloads, and the benchmark targets for call-cost measurement.

The integration suite compiles embedded sample scripts without launching GUI
applications. Running HelloKern exercises its complete host workflow.
