# KernScript Scripting Cookbook

Each extern and script function declares its return kind. A valued result can
be assigned, passed to another call, or discarded as a statement.

## Pull-model I/O

```ks
extern read() -> value;
extern emit(x) -> void;
func main() -> void {
  var v = read();
  emit(v + 1);
}
```

## Nested calls and discarded results

```ks
extern read() -> value;
extern double_it(x) -> value;
extern emit(x) -> void;
func main() -> void {
  read();
  emit(double_it(read()));
}
```

The first result is discarded. The second call passes its result directly
to `double_it`. Calls preserve earlier expression values, so expressions
such as `1 + read()` do not need temporary variables.

## Host state across calls

```ks
extern bump() -> void;
extern count() -> value;
extern emit(x) -> void;
func step() -> void {
  bump();
}
func report() -> void {
  emit(count());
}
```

The host can call `step` repeatedly and then call `report`. Each call starts
with clean VM stacks while host bindings preserve their state.

## Modules

`lib.ks`:

```ks
module lib;
export func twice(x) -> value {
  return x + x;
}
```

Root script:

```ks
import lib;
extern emit(x) -> void;
func main() -> void {
  emit(lib::twice(21));
}
```

Pass the library directory as a compile search path. Edit the module, compile
again, and load the bytecode into the same VM; host bindings remain registered.
