# KernScript Scripting Cookbook

Minimal patterns. Each ran end-to-end through
the real compiler with outputs asserted; broken
variants quote the actual diagnostic.

## Pull-model I/O

Scripts pull input through valued externs and
push results through void ones.

```ks
extern read();
extern emit(x);
func main() {
  var v = read();
  emit(v + 1);
}
```

`read()` returns 41; emits "42".

## Valued versus void calls

Bind every valued extern call. A bare valued
extern call is not a discard: mixing the two
forms fails the compile.

```ks
extern get_value();
extern emit(x);
func main() {
  var v = get_value();
  emit(v);
  var w = get_value();
  emit("done");
}
```

Emits "35", "done". Writing `get_value();` as
a statement too gives:

```text
t2.ks:6:3: error: Inconsistent extern use.
note: use it always as a value or always as
a statement
```

## Return chaining

Nest valued calls; each return flows into the
next call.

```ks
extern read();
extern double_it(x);
extern emit(x);
func main() {
  emit(double_it(read()));
}
```

Emits "82" (41 doubled).

Discipline: leftovers count toward the next
call, so hoist 0-argument externs out of larger
expressions. `while (y < hi())` threw "The
extern function 'hi' expects 0 arguments, but
1 were pushed." Binding `hi()` to a variable
first runs (see banding below).

## Host-held state across calls

The host owns long-lived state; scripts name
small steps. Each `callFunction` starts from
clean stacks but host bindings persist.

```ks
extern bump();
extern count();
extern emit(x);
func step() {
  bump();
}
func report() {
  emit(count());
}
```

Call `step`, `step`, `report`: emits "2".

## Banding long runs

One call runs to completion, so chunk big loops
and let the host pump between bands. Scripts
pull bounds through externs because
`callFunction` takes no arguments.

```ks
extern lo();
extern hi();
extern emit(x);
func run_band() {
  var top = hi();
  var y = lo();
  while (y < top) {
    emit(y);
    y = y + 1;
  }
}
```

Host sets lo/hi per band (0-3, 3-6, 6-9,
9-10) and calls `run_band` four times. Output
equals one 0-10 run (10 rows).

## Extern-use consistency

One extern, one form across the program.

```ks
extern emit(x);
func a() {
  emit(1);
}
func b() {
  emit(2);
}
func main() {
  a();
  b();
}
```

Emits "1", "2".

## Module layout: lib plus hot-reload

lib.ks:

```ks
module lib;
export func twice(x) {
  return x + x;
}
```

Root:

```ks
import lib;
extern emit(x);
func main() {
  emit(lib::twice(21));
}
```

Compile the root with search paths holding the
lib directory; emits "42". Edit lib (add 1),
recompile, `load` again on the same VM: emits
"43" with no re-register, since `load` keeps
host bindings.

Verified: ran `.scratch/docs_verify/driver.exe
snippets` (ALL-OK, 32 checks).
