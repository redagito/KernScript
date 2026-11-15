# LifeLab

LifeLab is an interactive cellular automata editor. The host owns the grid and files; KernScript defines each cell's next state.

## Use and files

Build and run `LifeLab` with GUI samples enabled. Grid dimensions are 8–160 cells per side. Paint cells, choose a preset or randomize from a numeric seed, then step or run. Finite or toroidal boundaries can be selected. Resizing preserves the overlapping cells and asks before discarding cells beyond a smaller edge. Save/Open use versioned `.life` patterns. CSV experiments (1–10,000 generations) run from a private copy of the current board and rule, leaving the open pattern unchanged.

The text pattern format is a header `KernScript LifeLab 1`, a line with width, height, generation and wrap flag (`0` finite, `1` toroidal), then one row of width `0`/`1` characters per grid row. Extra content and malformed rows are rejected.

## Rules

Rule scripts declare `cell_alive() -> value`, `neighbors() -> value`, `emit(next) -> void`, and `func decide() -> void`. `cell_alive()` returns 0 or 1; `neighbors()` returns 0–8 under the selected boundary mode. `emit` must be called once with 0 or 1. Each cell call has a 10,000-instruction budget. Before activation, a rule is checked for all 18 cell-state/neighbors combinations. A generation commits only after every cell succeeds.

Load or reload `.ks` rules from the `rules/` directory; rules may import helpers from their own directory. A failed reload keeps the active rule. For example, copy `rules/highlife.ks` and change its birth condition to `count == 2` with no survival condition to make a Seeds variant.

`LifeLab --verify --test-root <directory>` runs headless checks; CTest registers `LifeLab.Verify`.
