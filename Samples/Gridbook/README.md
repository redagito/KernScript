# Gridbook

Gridbook is an editable formula workbook. C++ owns cells, dependency tracking, undo, storage and the table; KernScript evaluates formulas and implements range aggregates in `scripts/sheet.ks`.

## Workbook use

Build and run the `Gridbook` target with GUI samples enabled. Edit a selected cell or formula and press Enter or **Set**. Save/Open use versioned `.gridbook` files; **Export CSV** writes displayed values, not formulas. Undo and Redo cover edits and dimension changes. Resizing cannot discard populated cells; clear them first.

Formulas begin with `=` and support arithmetic, parentheses, and references such as `B12` or `AA40`. `SUM`, `AVG`/`AVERAGE`, `MIN`, `MAX` and `COUNT` accept a cell or rectangular range. Aggregates skip empty/text cells and return zero if no numeric cells exist. Direct references to empty/text cells return zero; errors propagate. Addresses and aggregate names are case-insensitive. Formula arithmetic uses float32, not decimal financial arithmetic.

## Helpers and limits

Edit `scripts/sheet.ks` beside the executable and select **Reload helpers**. Existing formulas are checked against candidate helpers before replacement. Add an exported helper, for example `discount(amount, percent)`, then use `=sheet::discount(A1, 10)` in a cell. Internal range bindings are `range_count(id)`, `range_value(id, index)` and `range_numeric(id, index)`; formulas use cell/range addresses instead.

Limits are 1–1000 rows, 52 columns (AZ), 10,000 cells, 4096 bytes per cell and 8 MiB per document. Undo retains up to 30 edits within an approximate 16 MiB budget. The dependency graph supports up to one million references; each formula has a finite VM instruction budget.

`Gridbook --verify --test-root <directory>` runs headless checks; CTest registers `Gridbook.Verify`.
