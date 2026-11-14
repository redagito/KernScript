# LifeLab

LifeLab is an interactive cellular automata lab. The grid is bounded to 8 through 160 cells per side. Click or drag to paint and erase, then step once or run continuously. The status line reports the active rule, generation, live population, births, and deaths from the last generation.

## Workflow

1. Choose Glider, Blinker, or Toad and apply it, or randomize from a repeatable numeric seed. Clear resets the board. Random boards use a 28 percent live-cell chance.
2. Paint cells directly. Click a live cell to erase it; drag to paint the chosen state.
3. Toggle finite edges or toroidal wrapping. Change playback speed with the slider.
4. Resize within 8..160. Existing cells in the overlapping rectangle are retained. Shrinking asks before discarding any cells beyond the new edge.
5. Save and open `.life` patterns. Save As creates another copy. Unsaved changes prompt before replacing the pattern or closing the window. Files are strictly checked before changing the current board.
6. Load or reload a `.ks` rule. A failed reload leaves the current rule and board running. A failed generation stops playback and leaves the complete previous generation visible.
7. Enter an experiment length (1..10000) and export CSV. The experiment starts from a private copy of the current board and rule. It records the initial population and each generation's population, births, deaths, rule, and boundary mode. It refuses to write to the currently open pattern path.

## Pattern format

The UTF-8 text format is versioned and intentionally simple:

```text
KernScript LifeLab 1
width height generation wrap
one row of width 0/1 characters per grid row
```

`wrap` is `0` for finite edges and `1` for toroidal edges. Every row must have exactly the declared width, and extra content is rejected. Saves use an adjacent temporary file followed by an atomic replacement.

## Custom rules

A rule script declares three host functions and a `decide()` entry point:

```ks
extern cell_alive() -> value;
extern neighbors() -> value;
extern emit(next) -> void;

func decide() -> void {
  var alive = cell_alive();
  var count = neighbors();
  if (alive == 1) {
    if (count == 2 or count == 3) { emit(1); } else { emit(0); }
  } else {
    if (count == 3 or count == 6) { emit(1); } else { emit(0); }
  }
}
```

`cell_alive()` returns 0 or 1 for the cell being evaluated. `neighbors()` returns a count from 0 to 8, respecting the selected boundary mode. `emit(next)` must be called exactly once with 0 or 1. Before a rule replaces the active one, LifeLab compiles and loads it, checks all 18 combinations of cell state and neighbor count, and checks the output protocol. Every generation is computed into a separate buffer and committed only after every cell succeeds. The VM instruction budget also stops non-terminating rules.

The `rules` folder includes Conway, HighLife, and DayNight examples. Choose **Load / reload rule** and open one of those files or a modified copy.

The app starts by loading the external `rules/conway.ks` beside the executable,
with a repository fallback for source-tree runs. Editing pauses playback. A rule
can import helper modules from its own directory. Each cell call has a
10000-instruction budget, and rule errors include compiler diagnostics.

## Compare a larger experiment

Resize to 80 by 80, set seed `1234`, and choose **Randomize**. Save this starting
pattern as `seed-1234.life`. Load `rules/conway.ks`, enter `100` generations and
export `conway.csv`. Then load `rules/highlife.ks` and export `highlife.csv`.
Experiments leave the starting board untouched, so both runs use the same
population and boundaries. Compare population and births over time to see how
the extra birth condition changes the result. Repeat with toroidal wrapping to
separate rule behavior from cells dying at the edge.

For a useful customization, copy HighLife and change its birth condition to
`count == 2`, with no survival condition. This is the Seeds rule: live cells
always die and empty cells with two neighbors become alive. Save it as
`seeds.ks`, load it and compare another CSV from the same pattern. The scripts
own the transition rule; the host still handles editing, files and experiments.

## Running and verification

Build the `LifeLab` target with GUI samples enabled. Then run the executable normally to open the app. `LifeLab --help` prints command line options. `LifeLab --verify --test-root <project-path>` runs headless checks before any FLTK window is created. CTest registers this as `LifeLab.Verify` and uses `build/test-tmp/LifeLab` under the project.

The verifier exercises glider and blinker behavior, finite and wrapped edges, display-to-cell mapping, resize preservation, pattern round trips and rejection, safe rule replacement, transactional runtime failures, scratch CSV experiments, and an 80 by 80 board over multiple generations.
