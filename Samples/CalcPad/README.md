# CalcPad

CalcPad is a command-line expression workpad. It compiles each expression and evaluates it in a bounded KernScript VM.

## Run

Run `CalcPad` for an interactive prompt. `let name = expression` stores a value; `:import MODULE` loads a helper module; `:save FILE` saves the session and `:quit` exits. Restore with `CalcPad --open FILE`. `CalcPad --run FILE` processes one expression or command per line; blank lines and `#` comments are ignored. Batch files may use `let`, `:import`, `:save` and `:open`. See `CalcPad --help` for options.

Modules are found in `scripts/` beside the executable, `scripts/` in the current directory, and paths passed with `--library DIR`. A module such as `finance.ks` declares `module finance;`; call exports as `finance::payment(...)`. The bundled `math_helpers.ks` exports `percent(amount, rate)` and `compound(principal, annualRatePercent, periods)`.

## Data and limits

Each calculation is a single-line expression. Variables are finite float32 host bindings, so stored values have float32 precision; direct expressions follow KernScript integer and float rules. The VM allows 100,000 instructions per calculation. Session and batch files are limited to 4 MiB, lines to 64 KiB and stored tables to 10,000 records. Sessions are readable, versioned text; opening validates the full session and imported modules before replacing current work.

For a repeatable estimate, put assumptions and derived values in a batch file and run `CalcPad --run estimates.txt`. Re-run dependent expressions after changing an input; assignments do not recalculate prior results.

`CalcPad --verify` runs the headless verifier. CTest registers `CalcPad.Verify`.
