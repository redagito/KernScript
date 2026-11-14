# Daybook

Daybook is a personal task list with editable ranking rules. The task list and files belong to the C++ host. KernScript decides each task's score and explanation, and the host displays the results from highest score to lowest.

## Build and run

Enable GUI samples in the project configuration, build the `Daybook` target, then run it from its build directory. The app starts with an empty list. Add a task using the fields on the right, then select a row to edit, mark it complete, or delete it. Save writes a `.daybook` file; Open and New prompt about unsaved edits. Closing the window does the same.

The deadline field accepts `YYYY-MM-DD`. Empty deadlines have no due-date boost. The default policy boosts overdue, due-today, and due-soon tasks, and pushes completed tasks below open work. The due-day value is calculated from the host's local date and refreshed when the list is ranked. **New task** clears the editor; **New list** starts a new document. Leaving unapplied editor changes offers to apply them, discard them, or keep editing.

## Ranking policy

Policies are ordinary `.ks` files under `policies/`. Edit `policies/urgent.ks` with a text editor and use **Reload active policy** to recompile it, or **Choose policy** to load another file without rebuilding C++. If compilation or loading fails, Daybook keeps using its last working policy.

The policy entry point is `func rank() -> void`. For each task, the host exposes:

```text
priority()  -> value    // integer 1 through 5
kind()      -> value    // task kind, such as bug, feature, chore
tags()      -> value    // raw tag text
has_tag(tag) -> value   // exact tag in a comma-separated list, ignoring surrounding spaces
minutes()   -> value    // estimate in minutes
completed() -> value    // 1 when complete, otherwise 0
due_days()  -> value    // days from today; 9999 means no/invalid deadline
emit(score) -> void     // return the numeric ranking score to the host
explain(text) -> void   // append a human-readable reason part for the task
```

The host calls `rank()` once per task. Each call must emit exactly one score and at least one explanation; multiple explanation parts are joined for display. Scores sort stably, so tasks with equal scores retain their original order. The host owns task data, stable IDs, completion state, file storage, and sorting. Policies own scoring and explanations. `edit` replaces the full task record, while the app editor carries forward the current completion state.

## Task file

Daybook stores a versioned text format containing stable task IDs, title, priority, kind, tags, estimate, completion state, and deadline. Save writes a temporary sibling file and replaces the destination only after a complete write. Open validates the full file into a candidate model before changing the current list, so a malformed file leaves the in-memory tasks untouched.

Documents are limited to 50000 tasks and 8 MiB of serialized data. The storage
limit is checked when adding or editing tasks, so an accepted document remains
saveable. For a useful customization, add `extern has_tag(tag) -> value;` to a
policy and boost tasks tagged `today`, adding an `explain("Tagged today")`
reason. The host stores and matches tags while the script decides their weight.

## Verification

A realistic larger project is a release backlog spanning bugs, features and
chores. Use deadlines for milestones, estimates for planning time and tags for
work streams. Save the backlog, then compare a deadline-focused policy with one
that boosts today's tagged work. Completion and stable IDs stay in the host
document while scripts change the planning order and explain their decisions.
The verifier builds a 1200-task list to exercise that scale without manual data
entry.

With testing enabled, configure dependencies and GUI samples using the root
build instructions, then build `Daybook`. The `--verify` mode runs the real
model and script host without constructing FLTK widgets. CTest invokes the
same executable headlessly. For a build directory named `build`:

```powershell
cmake --build build --config Release --target Daybook
ctest --test-dir build -C Release -R Daybook.Verify --output-on-failure
```

To run the verifier directly, supply an isolated test root:

```powershell
build\Samples\Daybook\Release\Daybook.exe --verify --test-root build\test-tmp\Daybook
```

The verifier exercises task creation, editing, completion and deletion; invalid task and file rejection; policy changes and failed reload preservation; full save/open roundtrip with spaces and quotes; failed-save data preservation; and deterministic ranking over more than 1,000 tasks. Its uniquely named fixtures are created only beneath the supplied test root.
