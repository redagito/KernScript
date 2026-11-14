# Gridbook

Gridbook is an editable formula workbook. C++ owns the cells, dependency graph,
undo history, storage, and table. KernScript evaluates formulas and implements
the range aggregates in `scripts/sheet.ks`.

## Use a workbook

The initial invoice is a working document with quantities, prices, totals, and
an average. Select a cell, edit its text or formula in the cell field, and press
Enter or **Set**. Dependent cells update automatically. Select **Budget** or
**Blank** and click **New** to start another workbook.

**Save** and **Open** preserve cell text, formulas, and dimensions in a readable
`.gridbook` document. **Save As** creates another workbook. **Export CSV** writes
the displayed values for use in other applications. CSV exports do not retain
formulas. Undo and Redo apply to cell edits and dimension changes; undo history
lasts for the current session.

Enter row and column counts and click **Resize sheet** to grow the workbook.
Shrinking rejects removal of populated cells, so clear those cells first.
Click a column boundary to adjust its display width. The status line shows the
selected cell's references or error details.

## Formulas and scripts

Formulas start with `=` and support arithmetic, parentheses, cell references
such as `B12` or `AA40`, and these functions:

| Formula | Behavior |
| --- | --- |
| `=SUM(B2:B20)` | Sum numeric cells in a rectangular range |
| `=AVG(B2:B20)` or `=AVERAGE(B2:B20)` | Average numeric cells |
| `=MIN(B2:D20)` / `=MAX(B2:D20)` | Smallest / largest numeric cell |
| `=COUNT(B2:B20)` | Count numeric cells |
| `=sheet::percent(D4, 12.5)` | Call an ordinary exported script helper |

Aggregate functions accept one cell or one rectangular range. They skip empty
and text cells and return zero when no numeric cells exist. Direct references
to empty or text cells yield zero. References to cells with errors propagate
an error. Addresses and aggregate function names are case insensitive.

Formula numbers and numeric cell values use KernScript float32 arithmetic.
In particular, `=1/2` evaluates to `0.5`. This workbook does not provide decimal
financial arithmetic. Formula text is an expression, not a script statement.

Edit `scripts/sheet.ks` beside the executable and click **Reload helpers**.
Existing working formulas are compiled against replacements before switching
programs. A compilation failure keeps the previous programs and results.
For a useful customization, add an exported numeric helper, such as
`discount(amount, percent)`, then call `sheet::discount(A1, 10)` in a cell.
The existing `percent` helper shows the complete pattern.

Range helpers call host `range_count(id)`, `range_value(id, index)`, and
`range_numeric(id, index)`. The host derives the range IDs and dependency edges
from the formula. The script owns the aggregation loops and arithmetic.
These host functions are internal to generated formulas; users reference cells
and ranges by their ordinary addresses.

Formula errors are displayed in the cell. `#CYCLE!` identifies a circular
reference; cells depending on that cycle show `#DEPEND!`. Invalid expressions
show `#FORMULA!`, and runtime or nonnumeric result errors show `#VALUE!`.
Correcting an expression recalculates its dependents. Broken formula examples
are exercised by the verifier rather than included in the initial invoice.
If recalculation cannot finish, formula cells show `#RECALC!` instead of stale
values, and CSV export retries calculation before writing. Reduce the workload
and press **Recalculate** to retry. Documents retain their text and formulas
even when a calculation fails.

## Larger projects and verification

Workbooks support 1 to 1000 rows, up to 52 columns (AZ), at most 10000 cells,
4096 bytes of text per cell, and an 8 MiB document. Undo history retains up to
30 edits within an approximate 16 MiB budget. Each formula has a finite VM
instruction budget. Unchanged compiled formulas are cached, and ordinary edits
only execute the affected dependency closure. A helper reload recalculates all
formulas.
The dependency graph is limited to one million references. Candidate graph
changes are validated before replacing cached plans, so a rejected edit can be
corrected without reusing old values as results for new formulas.

A realistic larger project is a 500-item invoice with per-item totals, a
running total, and a range summary. The headless verifier builds that workbook
and runs 1001 formulas, then checks that an edit reuses the compiled programs
and recalculates just the affected cells.

```text
Gridbook --verify --test-root build/test-tmp/Gridbook
```

This command creates no window. CTest registers it as `Gridbook.Verify`. It
also exercises save/open, escaped text, error propagation, cycles, resizing,
CSV export, helper customization, and failed reload preservation. Unit tests
cover the document model and text serialization.
