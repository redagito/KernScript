# Toolsmith

Toolsmith is a small FLTK host for KernScript-defined text tools. The bundled
Text Formatter handles multiline text with uppercase, lowercase, edge trimming,
whitespace normalization and find/replace. Notes provides a title and a
multiline body. Both use versioned `.kstool` documents with named fields.

## Run

`Toolsmith` opens the Text Formatter. Use the toolbar to create a new document,
open, save, save as, select either bundled tool, load a custom script, or reload
the active script. Opening a document, changing tools, or starting a new
document prompts before discarding unsaved edits. A failed open or script
reload leaves the current fields and tool active.

`Toolsmith --script path/to/tool.ks` loads a custom script. `--open file.kstool`
opens a document for the selected bundled or custom tool. `--help` lists the
options. `--preview path.png` renders only the application's own widgets to an
offscreen image without showing the window.

## Script API

Scripts expose `func build() -> void`. Call `tool(name, script_id)` once, then
declare `field(name, label, kind)` and `button(name, label, handler)`. Field
kinds are `no` for a single-line editor, `yes` for a multiline editor, and
`output` for a read-only multiline result. Button handlers are zero-argument
void functions. `get_text` and `set_text` access named fields; a handler's
field changes are staged and committed only when it returns successfully.

Text helpers are `upper`, `lower`, `trim`, `normalize`, and `replace`. Replace
requires a nonempty search string. The host owns widget creation, field state,
document encoding, and file operations; scripts define the tool identity,
fields, buttons, and action logic. Documents store the tool name, stable script ID,
and every field value under a versioned header. Unknown, duplicate, missing,
or malformed fields are rejected.

Field, button and script IDs use letters, digits, dots, underscores and hyphens,
up to 80 characters. The display name and labels can contain spaces. Documents
are limited to 8 MiB, and each script entry has a 100000-instruction budget.
Saving cannot replace an existing KernScript source file.

Reload preserves matching named values and rebuilds the visible controls after
a successful candidate build. It rejects a changed tool identity or removal of
a populated field. Save the document and use **Load Script** when intentionally
changing the schema in either of those ways. Stable script IDs allow a document
to move between copies of the same tool without depending on a build directory.

## Customize a useful tool

Copy `ui.ks` as a ticket-cleaning tool, change its `tool` identity, and add a
**Clean ticket** button. Its handler can replace a known customer identifier,
trim pasted log text and write the result to `output`. The script chooses the
sequence and validation; the host supplies text primitives and persistence.
Keep field IDs stable while adjusting labels or adding compatible controls.

For a larger project, paste a 2000-line incident log into the formatter, replace
hostnames or identifiers, normalize the relevant excerpt, then save the input,
options and result as a reusable document. Use Notes for an accompanying research
or meeting journal with a title and multiline body. The verifier processes a
2000-line formatter input and a 2000-line note through the same model functions
used by the UI, including save/open restoration.

## Verification

Build the `Toolsmith` target through the project's normal Conan/CMake setup with
GUI samples enabled. Run `Toolsmith --verify --test-root <directory>` to execute
both bundled tools without opening a window. It checks text transforms,
validation, multiline and large inputs, document overwrite and roundtrip,
invalid-document preservation, staged handler rollback, and compile/build
reload failures. The CTest entry uses `build/test-tmp/Toolsmith` for fixtures.
