# KernScript applications

Build with `KERNSCRIPT_BUILD_SAMPLES=ON` for the command-line and native
examples, and `KERNSCRIPT_BUILD_GUI_SAMPLES=ON` for the FLTK applications.
The corresponding Conan options are `with_samples` and `with_gui_samples`.
Folders, build targets, and executable names match.

| Application | What it demonstrates |
| --- | --- |
| [HelloKern](HelloKern/README.md) | Smallest compile, bind, load, and execute workflow |
| [NativeEvents](NativeEvents/README.md) / [FltkEvents](FltkEvents/README.md) | Native and portable event handling tutorials |
| [CalcPad](CalcPad/README.md) | Calculation workpad with variables, helper modules, and saved sessions |
| [TextFlow](TextFlow/README.md) | Streaming file and pipe processing with external rules |
| [PolicyLab](PolicyLab/README.md) | Explained batch pricing and policy comparisons in integer cents |
| [Canvas](Canvas/README.md) | PNG editing, undo, scripted brushes, filter previews, and generators |
| [Gridbook](Gridbook/README.md) | Editable, saved workbooks with formula ranges and dependency recalculation |
| [Cadence](Cadence/README.md) | Saved multi-bar compositions, editable notes, script generation, and WAV export |
| [Daybook](Daybook/README.md) | Persistent task planning with editable ranking policies and explanations |
| [SignalDesk](SignalDesk/README.md) | Saved monitoring layouts, recorded feed replay, chart statistics, and alert policies |
| [Storybook](Storybook/README.md) | Connected branching fiction, persistent choices, history, and script authoring |
| [LifeLab](LifeLab/README.md) | Editable saved automata patterns, external rules, and population experiments |
| [Toolsmith](Toolsmith/README.md) | Script-defined multiline text tools and notes with saved documents |
| [Dialogue](Dialogue/README.md) | Authorable support and NPC conversations, saved replay, and rule/state inspection |

For a Visual Studio Release build, an executable is located at
`build/<build-folder>/Samples/<Application>/Release/<Application>.exe`.
Other generators generally omit the `Release` directory. Script modules
are copied beside applications that load them from disk. Run the executable
from its own directory, or use its repository-relative script fallback.

See [the roadmap](../docs/Sample%20Roadmap.md) for the staged upgrade scope,
old-to-new name mapping and validation record. Each application README describes
its shipped workflow, script API and a useful customization.

## Verification and layout previews

The full sample configuration adds twelve application workflow verifiers to the
library suites in CTest. The larger apps exercise their production models,
VM bindings, document formats and
failure handling without opening windows. On Windows, ten additional layout
checks render the nine FLTK applications and the separate Notes tool to
`<build-folder>/sample-previews/`. Those checks draw only each app's own widget
hierarchy into an offscreen image. They do not capture the desktop or show
native windows.

The nine larger GUI apps accept `--preview output.png` for the same isolated
rendering. Workflow verifiers and previews serve different purposes: the former
check application behavior, and the latter make widget layout review repeatable.
HelloKern and the two event-binding tutorials remain compact introductions.
