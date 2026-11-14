# TextFlow

TextFlow is a small streaming text processor driven by a KernScript rules file. It reads one line at a time, invokes `process()` once per line, and writes only text explicitly passed to `emit`. The host keeps one current line in memory, so memory use does not grow with the input file size. A very long individual line still needs memory proportional to that line.

## Build and run

Build the `TextFlow` target with the sample applications enabled. The build copies `rules.ks` and `sample.log` beside the executable.

```text
TextFlow --help
TextFlow --input sample.log --script rules.ks
TextFlow --input sample.log --output filtered.log --script rules.ks
type sample.log | TextFlow --script rules.ks
```

`--input` defaults to standard input and `--output` defaults to standard output. `--script` is required. TextFlow checks for input/output and script/output collisions, including hard links. For a file output, it writes to a uniquely named temporary file beside the destination, flushes and closes it, then replaces the destination only after every input line succeeds. An existing output stays intact after script, runtime, read, or write failures. The script is compiled, loaded, and bound before output staging begins. Diagnostics go to standard error.

## Host API

Every rule file defines a zero-argument, void entry function named `process`:

```ks
func process() -> void {
  // Inspect the current record and optionally call emit(...).
}
```

TextFlow calls this function once for every input line. The VM instruction budget resets for each call. The current line stays unchanged throughout that call. Available host functions are:

| Declaration | Behavior |
| --- | --- |
| `extern contains(part) -> value;` | Returns `1` when `part` occurs in the current line, otherwise `0`. |
| `extern line() -> value;` | Returns the current line without its line ending. CRLF input is normalized by removing the trailing carriage return. |
| `extern upper() -> value;` | Returns the current line converted to uppercase using the C locale. |
| `extern replace(from, to) -> value;` | Returns the current line with every non-overlapping occurrence of `from` replaced by `to`. An empty `from` leaves the line unchanged. |
| `extern line_number() -> value;` | Returns the one-based input line number as a 32-bit unsigned integer. Processing fails with a diagnostic if the input exceeds that range. |
| `extern emit(text) -> void;` | Writes `text` followed by a newline. Calling it multiple times emits multiple output lines. |

The bundled `rules.ks` drops debug records, uppercases errors, and normalizes the `info:` prefix. Edit or replace the rules file to change the policy.

A useful customization is to skip routine `/health` requests before the other
branches, using `contains("/health")` and returning without emitting. The same
stream can then produce a smaller service log while retaining the existing
error highlighting and prefix cleanup.

## Larger workload

Point the same streaming command at a separate large log file; TextFlow does not preload the file:

```text
TextFlow --input archived-service.log --output archived-errors.log --script rules.ks
```

## Deterministic verifier

When built with `BUILD_TESTING`, CTest registers `TextFlowVerifier`. It can also be run directly:

```text
TextFlow --verify
```

The verifier exercises the bundled rules, script compiler, VM, host functions, stdin/stdout processing, replacement, line numbering, CRLF normalization, and empty input. It also checks that compile failures, missing entry functions, runtime errors, and write failures are handled, that existing output survives processing failures, and that path and hard-link aliases are rejected. It writes each run's fixtures under a unique directory inside `build/test-tmp/TextFlow` relative to the project root.
