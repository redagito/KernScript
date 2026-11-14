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

## Discard a valued call

The first `read()` result is intentionally discarded; the second is passed
to `emit`.

```ks
extern read() -> value;
extern emit(x) -> void;
func main() -> void {
  read();
  emit(read());
}
```

## Nested calls

```ks
extern read() -> value;
extern double_it(x) -> value;
extern emit(x) -> void;
func main() -> void {
  emit(double_it(read()));
}
```

Calls preserve earlier expression values, so `1 + read()` and
`combine(1, read())` work without temporary variables.

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

## Return paths

Every reachable path in a value function must return a value:

```ks
func abs(x) -> value {
  if (x < 0) {
    return 0 - x;
  } else {
    return x;
  }
}
```

Void functions use `return;` or reach the end of the body. A void call cannot
be used as a value.

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
