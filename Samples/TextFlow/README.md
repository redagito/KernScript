# TextFlow

TextFlow streams input one line at a time through a KernScript rules file. It writes only text passed to `emit`; memory use is proportional to the current line, not total input size.

## Run

Build `TextFlow` with sample applications enabled; the build copies `rules.ks` and `sample.log` beside the executable.

```text
TextFlow --help
TextFlow --input sample.log --script rules.ks
TextFlow --input sample.log --output filtered.log --script rules.ks
type sample.log | TextFlow --script rules.ks
```

`--script` is required; input defaults to stdin and output to stdout. TextFlow rejects input/output and script/output collisions, including hard links. File output is staged beside the destination and replaced only after all lines succeed. Failures preserve existing output; diagnostics go to stderr.

## Rule API

Each rule file defines `func process() -> void`; TextFlow calls it once per line with a reset VM instruction budget. The current line remains unchanged during the call.

| Declaration | Behavior |
| --- | --- |
| `contains(part) -> value` | 1 if part occurs in the line, otherwise 0 |
| `line() -> value` | Current line without its ending; CRLF is normalized |
| `upper() -> value` | Uppercase using C locale |
| `replace(from, to) -> value` | Replace non-overlapping occurrences; empty `from` leaves line unchanged |
| `line_number() -> value` | One-based 32-bit unsigned line number |
| `emit(text) -> void` | Write text plus newline; may be called multiple times |

Declare host functions with `extern`, for example `extern contains(part) -> value;`. The bundled rules drop debug lines, uppercase errors and normalize the `info:` prefix. To skip health checks, return without emitting when `contains("/health")` is true.

`TextFlow --verify` runs the headless verifier; CTest registers `TextFlowVerifier`.
