# CalcPad

CalcPad is a small command-line workpad for individual calculations. It compiles each expression with the KernScript compiler, then runs the bytecode in a bounded VM.

## Run it

Start `CalcPad` for an interactive prompt. Enter arithmetic directly, or name a reusable value:

```text
> 2 + 3 * 4
= 14.0
> :import math_helpers
imported math_helpers
> let subtotal = 89.95
subtotal = 89.9499969
> let tax = math_helpers::percent(subtotal, 8.25)
tax = 7.4208746
> subtotal + tax
= 97.370872497558594
> :save lunch.calc
session saved
> :quit
```

Use `CalcPad --open lunch.calc` to restore variables and calculation history. `CalcPad --run estimates.txt` runs a plain text file, one expression or workpad command per line. A batch file can use `let`, `:import`, `:save`, and `:open` too. Lines beginning with `#` and blank lines are ignored. See `CalcPad --help` for command syntax.

## Helper modules

Use `:import MODULE` to make an external KernScript module available to calculations. The compiler looks for `MODULE.ks` in `scripts/` beside CalcPad, `scripts/` under the current directory, and directories supplied with `--library DIR`. For example, create `finance.ks` with `module finance;` and exported value functions, then run `CalcPad --library ./my-kern-modules` and enter `:import finance`. Calls use the module namespace, such as `finance::payment(principal, rate, months)`. `math_helpers.ks` is bundled and provides `percent(amount, rate)` and `compound(principal, annualRatePercent, periods)`.

## Limits

Each entry is one single-line expression. `let name = expression` creates or updates a finite numeric variable after successful evaluation. Named variables are supplied through checked float32 host bindings, so they have float32 precision even when their original expressions produced integers. Their full float32 range and signed zero survive reevaluation and session round trips. Direct expressions retain KernScript's ordinary integer and float rules, and printed results preserve enough digits to represent the returned VM value. Variables and history live in memory until saved. Session files are readable, versioned text; opening one validates the entire file, including imported modules, before replacing the current workpad. Session and batch files are capped at 4 MiB, a line at 64 KiB, and stored tables at 10,000 records. The VM stops a calculation after 100,000 instructions. CalcPad reports compile diagnostics with the generated calculation source name. It is intended for numeric work, not as a general KernScript editor or arbitrary script runner.

## A larger calculation project

The bundled `scripts/renovation.txt` estimates a room renovation from dimensions,
paint coverage, labour, waste, three flooring quotes, tax and contingency.
It creates named intermediate values so the final totals can be traced back to
the assumptions. Run it from the executable directory:

```text
CalcPad --run scripts/renovation.txt
```

For a reusable session, start CalcPad, paste or enter the batch's commands and
use `:save renovation.calc`. Alternatively, add `:save renovation.calc` to your
copy of the batch, run it, and reopen the session with `--open renovation.calc`.
Change the dimensions, hourly rate or flooring prices in a copy of the batch
and rerun it to compare quotes. A useful helper customization is to add a
supplier-specific discount function to `math_helpers.ks` and apply it before
tax. Reassigning a variable does not automatically recalculate values that were
previously derived from it; rerun the relevant expressions or the whole batch.

## Verify

The headless `--verify` mode covers arithmetic precedence, variables, failed calculations preserving variable values, session save and restore, imported helpers, and batch processing. CTest registers this verifier as `CalcPad.Verify`.
