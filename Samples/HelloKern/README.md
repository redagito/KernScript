# HelloKern

HelloKern is the minimal embedding example. Enable command-line samples, build the `HelloKern` target and run it. It prints `hello from script` and `42`.

In `source/main.cpp`, the host compiles and assembles the embedded script, binds C++ `println(std::string)`, loads the VM, then calls `main() -> void`. The script performs arithmetic and requests output; C++ owns the VM lifetime. Change `40 + 2` in the embedded source and rebuild. To expose another capability, declare a typed `extern` in the script and bind the corresponding C++ function.

See [TextFlow](../TextFlow/README.md) for external rules and [Canvas](../Canvas/README.md) for reloadable modules.
