# Sample application roadmap

The samples should be useful applications with meaningful script extension
points. Larger real projects should exercise the VM naturally. Compact
introductions remain compact.

## Names and migration

Folder names and executable targets use the same name. Old executable names
are intentionally removed rather than kept as compatibility aliases.

| Previous name | Application | Purpose |
| --- | --- | --- |
| HelloScript | HelloKern | Minimal embedding tutorial |
| Calc | CalcPad | Calculation workpad |
| TextPipe | TextFlow | Streaming text processing |
| RuleEngine | PolicyLab | Editable business policies |
| ScriptGui | NativeEvents | Minimal Win32 event binding |
| ScriptFltk | FltkEvents | Minimal FLTK event binding |
| ScriptPaint | Canvas | Scriptable image editing |
| ScriptSheet | Gridbook | Formula workbook |
| ScriptMusic | Cadence | Algorithmic music composition |
| ScriptBoard | Daybook | Personal task planning |
| ScriptDash | SignalDesk | Configurable monitoring |
| ScriptNovel | Storybook | Interactive fiction |
| ScriptLife | LifeLab | Cellular automata exploration |
| ScriptUi | Toolsmith | Script-defined small tools |
| ScriptShrink | Dialogue | Rule-based conversation authoring |

## Rollout and acceptance

1. **Naming and foundation.** Migrate folders, build targets, script lookup
   paths, titles, docs, and tests. Build every renamed application.
2. **Canvas and TextFlow.** Canvas needs image open/save, transaction-based
   undo/redo, configurable brushes, adjustable filter preview, and useful
   image sizes. TextFlow needs file/stdin streaming, external rules,
   file/stdout output, useful primitives, and runnable example data.
3. **Gridbook, Cadence, and Daybook.** Complete a workbook save/open workflow
   with editable dimensions and formula aggregates; a saved multi-bar musical
   composition with note editing and WAV export; an editable, persistent task
   list with customizable ranking and explanations.
4. **SignalDesk, Storybook, and LifeLab.** Deliver saved monitoring layouts
   and recorded feeds; a complete branching story with saves and history;
   editable, saved automata patterns with rule exploration and statistics.
5. **Toolsmith, Dialogue, CalcPad, and PolicyLab.** Provide complete small
   script-defined utilities; stateful authorable conversation scenarios;
   saved calculation sessions; editable policy inputs and explained results.
   Preserve HelloKern and the two event tutorials as focused introductions.
6. **Documentation and workloads.** Each app documents its user workflow,
   script API, at least one worthwhile customization, and a realistic larger
   project. Keep internal profiling optional and errors useful to the user.

Every implementation stage must preserve user data when loading scripts or
documents fails, cover changed behavior with meaningful regression tests,
and pass affected application verification. Final acceptance requires every
application to build, its documented workflows to be exercised, and the full
test suite to pass in Release and AddressSanitizer. GUI checks must avoid
desktop capture and should use headless verification wherever practical.

This document tracks intended scope. Its presence does not prove a stage is
implemented or verified.

## Verified checkpoint: 2026-11-14

The naming migration is implemented for all 15 sample folders and executable
targets, runtime script lookup paths, titles, and existing documentation.
Runtime assets now update during builds even when no C++ file changed.

The first application stage is implemented:

- Canvas has PNG open/save, document sizes and fitted display, bounded undo
  and redo, adjustable brushes and opacity, anchored line previews, custom
  colors, transactional filter previews, cancellation and runtime rollback,
  threshold adjustment, and working-program preservation on failed reload.
- TextFlow streams files or stdin through external per-line rules, writes
  files or stdout, reports diagnostics, and stages file output so failures
  preserve existing data. Its bundled log and rules demonstrate a complete
  processing workflow.
- CalcPad was brought forward into this stage. It provides a calculation
  prompt, named values, helper imports, history, batch commands, and validated
  session save/open. Candidate imports and sessions are checked without
  replacing current state.

On Windows, all 15 applications built in both Release and AddressSanitizer.
Both configurations passed all six registered CTest entries: the three library
suites (401 cases, 2,429 assertions combined), plus the Canvas, TextFlow, and
CalcPad headless verifiers. Build and test logs are in
`build/task-tmp/samples-stage1-*-build.log` and
`build/task-tmp/samples-stage1-*-tests.log`.

Interactive GUI verifiers and visual layout inspection were not run during
this stage. The remaining applications still need the workflow upgrades in
stages 3 through 5, their application-specific documentation and realistic
larger projects, and final verification after those changes. The overall goal
is still active.

## Second application checkpoint

Stage 3 is implemented and verified through headless workflows:

- Gridbook is an editable workbook with invoice, budget, and blank templates;
  up to 10000 cells; saved documents and CSV export; range aggregates executed
  in scripts; undo/redo; cached compiled formulas and incremental dependency
  recalculation; cycle/error propagation; and transactional helper reloads.
  Its verifier includes a 500-item invoice with 1001 formulas and recovery
  from the one-million-reference graph limit. Failed recalculation is marked
  explicitly, and stale results cannot be exported as current values.
- Daybook provides task creation, editing, deletion and completion; stable
  IDs; deadlines, estimates and tags; saved documents; pending-editor and
  unsaved-document handling; and replaceable ranking policies with explanations.
  Policy validation retains the previous VM on failure. The model checks its
  storage budget while accepting edits, and the verifier ranks 1200 tasks.
- Cadence provides two-, four-, and eight-bar compositions, piano-roll note
  editing, track/instrument controls, root/scale/BPM settings, selected-range
  generation, document open/save, and WAV export. Theory and patterns live in
  script modules. Generation and reload are transactional, and hidden bars
  remain in saved documents. The verifier checks 128-step arrangements and
  audio bytes without playback.

CalcPad's named values now cross the host binding directly instead of being
inserted into generated source as numeric literals. Regression coverage checks
the full float32 range and signed zero. Gridbook normalizes spreadsheet number
notation to the decimal literals supported by the source language.

An independent correctness review identified and verified fixes for piano-roll
hit testing, deleting a new-task draft, and stale formula caches after a graph
limit failure. Targeted regression tests cover all three. Introductory README
files were also added for HelloKern, NativeEvents, and FltkEvents; HelloKern's
documented output was exercised.

All 15 applications built on Windows in both Release and AddressSanitizer.
Both configurations passed all nine CTest entries: 410 library test cases with
3645 assertions, plus six application verifiers. The final logs are
`build/task-tmp/samples-stage2-*-build.log` and
`build/task-tmp/samples-stage2-*-tests.log`. Formatting and whitespace checks
passed for the changed code.

Still pending: SignalDesk, Storybook, LifeLab, Toolsmith, Dialogue, and PolicyLab
workflow upgrades; their documentation and realistic larger projects; interactive
GUI and visual checks; and final suite verification after the remaining stages.
No GUI windows, desktop captures, or audio playback were used for this checkpoint.
The overall goal remains active.

## Final acceptance: 2026-11-15

The remaining application stages are implemented:

- SignalDesk has editable saved chart layouts, deterministic greenhouse
  simulation, recorded CSV import/export and prefix-only replay, selected-chart
  history, scripted statistics and hysteresis alerts, alert export, and candidate
  policy reload. Pending chart edits and unsaved recordings are handled before
  discard. Its verifier replays 2000 rows on six charts, checks rollback after a
  late-chart failure, and runs the full 10000-sample window.
- Storybook has a connected 26-scene story with five final endings, required
  choices, state across chapters, history, undo, saved journals, transcript
  export and transactional reload. Verification traverses 1097 reachable scene
  states and replays a separate 121-scene archive with 2420 dialogue lines.
- LifeLab has editable 8 through 160 cell grids, saved patterns, repeatable seeds,
  finite and toroidal boundaries, external rules, generation statistics, and CSV
  experiments on scratch copies. Verification checks each shipped rule's input
  table and an 80 by 80 simulation against an independent Conway calculation.
- Toolsmith provides script-defined multiline Text Formatter and Notes tools,
  stable tool documents, selection of bundled or custom tools, model-first UI
  construction, staged actions and validated reload with rebuilt widgets.
  Verification restores actual saved field content and processes 2000-line
  formatter and note documents.
- Dialogue is a stateful authoring playground with support and NPC scenarios,
  matched-rule inspection, isolated conversation state, undo, saved replay,
  transcript export and candidate reload. Verification includes a 600-message
  replay and failures after partial state changes.
- PolicyLab imports editable order batches and compares explained external
  pricing policies using integer cents and wide intermediate rounding. It
  writes complete reports atomically and verifies a 2000-order comparison.
- CalcPad now includes a runnable renovation estimate comparing three quotes.
  Its result and saved-session restoration were exercised, and the shipped
  batch is checked by its verifier.

Each application has a README covering its workflow, host/script boundary,
customization and an appropriate larger project. HelloKern and the two event
tutorials remain compact. HelloKern's current executable prints the documented
output. Integration coverage executes each event tutorial's actual embedded
script through clicks, milestones, reset and the next first click.

Independent review findings were fixed and checked: SignalDesk numeric editor
precision, Toolsmith whitespace replacement and independent document restoration
assertions, and data outputs accidentally overwriting script dependencies.
Offscreen visual inspection also found Gridbook controls clipped inside its
table group and Cadence help/status alignment. These are fixed. Gridbook's
layout check asserts that its bottom controls belong to the window.

### Evidence against the original rollout

| Requirement | Current evidence |
| --- | --- |
| Consistent names and folders | All 15 source directories, executable targets and runtime lookups use the final names; old names remain only in the migration table |
| Canvas and TextFlow workflows | Application verifiers cover PNG/document transactions, script tools and filters, streamed records and failed output preservation |
| Gridbook, Cadence and Daybook | Production model/VM verifiers cover saved documents, editing, aggregates or generation/ranking, reloads and larger projects |
| SignalDesk, Storybook and LifeLab | Production verifiers cover recordings, branching saves/history, saved patterns and scratch experiments |
| Toolsmith, Dialogue, CalcPad and PolicyLab | Production verifiers cover complete utility, conversation, calculation and pricing workflows |
| Documentation and runtime assets | Every app has a README; build asset targets copy scripts, helper modules and bundled example data on every build |
| GUI layout checks without desktop capture | Ten Windows CTest entries draw only application widget hierarchies into their own offscreen surfaces; all rendered layouts were inspected |
| Release and AddressSanitizer | All 15 apps build and all 25 CTest entries pass in both configurations |

The final Windows suite contains three library test executables with 410 cases
and 3874 assertions, twelve application workflow verifiers, and ten isolated
layout checks. Final logs are `build/task-tmp/samples-final-*-build.log` and
`build/task-tmp/samples-final-*-tests.log`. Layout images are generated under
`build/redesign-release/sample-previews/` and
`build/redesign-asan/sample-previews/`. Formatting and whitespace checks passed.
The project requires C++23.

GUI validation used offscreen rendering and production workflow verifiers.
Native file-dialog and physical button click-throughs were not automated.
No native app windows were shown for these checks, no audio was played, no
desktop was captured, and no D-drive paths were accessed. Task artifacts remain
inside the project tree. No implementation or required verification remains
from this rollout.
