# Daybook

Daybook is a task list with editable KernScript ranking policies. The host owns task records, IDs, completion state, files and sorting. Policies compute scores and explanations.

## Use

Build the `Daybook` target with GUI samples enabled. Add or edit tasks in the editor; Save writes a `.daybook` document. Open, New and close handle unsaved changes. Deadlines use `YYYY-MM-DD`; empty dates have no due-date boost and invalid dates are rejected. Due-day calculations use the host's local date when ranking.

## Ranking policies

Policies are `.ks` files in `policies/`. Edit `policies/urgent.ks` and select **Reload active policy**, or choose another file with **Choose policy**. A failed reload leaves the working policy active. The entry point is `func rank() -> void`, called once per task. Host functions are:

| Binding | Value or effect |
| --- | --- |
| `priority()` | Integer 1–5 |
| `kind()`, `tags()` | Task kind and raw tag text |
| `has_tag(tag)` | Exact comma-separated tag match; surrounding spaces ignored |
| `minutes()` | Estimate in minutes |
| `completed()` | 1 if complete, otherwise 0 |
| `due_days()` | Days from today; 9999 when no deadline is set |
| `emit(score)` | Emit the task's numeric score |
| `explain(text)` | Append a reason shown to the user |

Declare readers as `-> value`; `emit` and `explain` return `void`.

Each call must emit exactly one score and at least one explanation. Explanation parts are joined; equal scores retain original order. A policy can declare `extern has_tag(tag) -> value;` and add a score when `has_tag("today") == 1`, then explain the boost with `explain("Tagged today")`.

Versioned text documents store stable IDs, title, priority, kind, tags, estimate, completion and deadline. Open validates a complete candidate before replacing the current list. Limits: 50,000 tasks and 8 MiB serialized data; adding or editing checks that the document remains saveable.

`Daybook --verify --test-root <directory>` runs headless checks; CTest registers `Daybook.Verify`.
