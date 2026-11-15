# Toolsmith

Toolsmith hosts KernScript-defined text tools. The bundled Text Formatter transforms multiline text; Notes stores a title and body. Both use versioned `.kstool` documents.

## Run

Build `Toolsmith` with GUI samples enabled. `Toolsmith` opens Text Formatter; `--script path/to/tool.ks` loads a custom tool, `--open file.kstool` opens a document and `--help` lists options. `--preview path.png` renders the app widgets offscreen.

## Script API

Define `func build() -> void`. Call `tool(name, script_id)` once, then declare
`field(name, label, kind)` and `button(name, label, handler)`; these bindings
return `void`. Field kinds are `"no"` for single-line input, `"yes"` for
multiline input, and `"output"` for a read-only multiline result.

Button handlers are zero-argument void functions. `get_text(name) -> value`
and `set_text(name, text) -> void` access named fields. Changes commit only
if the handler succeeds.

Text helpers are `upper`, `lower`, `trim`, `normalize` and `replace`; replace requires a nonempty search string. The host owns widgets, field state, document encoding and files. Documents store tool name, stable script ID and all field values. Unknown, duplicate, missing or malformed fields are rejected.

Field, button and script IDs use letters, digits, dots, underscores and hyphens, up to 80 characters. Documents are limited to 8 MiB; each script entry has a 100,000-instruction budget. Reload preserves matching field values, but rejects changed tool identity or removal of populated fields. Save first and use **Load Script** for an intentional schema change. Keep IDs stable for document compatibility.

For example, copy `ui.ks`, change its tool identity and add a **Clean ticket** button that trims pasted log text and writes transformed output to an `output` field.

`Toolsmith --verify --test-root <directory>` runs headless checks; CTest registers `Toolsmith.Verify`.
