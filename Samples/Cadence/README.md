# Cadence

Cadence is a small algorithmic music workstation built with FLTK and KernScript. It keeps notes in a host-owned multi-bar composition while the three on-disk KernScript modules generate bass, percussion, and lead parts.

## Run it

Build and launch `Cadence`. Click a piano-roll cell to add a note at that pitch or remove the matching note. The roll shows two, four, or eight bars (32, 64, or 128 sixteenth-note steps). Select a track by clicking its button. Clicking the selected track button toggles mute. **Instrument +** cycles its three synthesized timbres.

Set **From step** and **To step** to choose an inclusive range, then use **Generate Track** or **Generate All** to regenerate that range. Generation replaces only the selected tracks and steps, and commits only if every script call succeeds. **Reload Scripts** compiles and validates the complete active arrangement in a candidate VM; a failed compile or runtime check leaves the previous VM and composition in place. Use **New**, **Open**, **Save**, and **Save As** for document work. Open and New ask how to handle unsaved edits, and closing the window does too. Open, Save As, and Export WAV use file choosers. The BPM input accepts 40–240 BPM. Root and scale cycle through the twelve chromatic roots and major, natural minor, and pentatonic scales. Changing theory controls changes subsequent script generation; existing notes remain until you regenerate them.

## Script host API

The host imports `bass`, `drums`, and `lead` from `music/` and exposes these functions to the root script:

- `cur_step() -> value` returns the current sixteenth-note index.
- `root() -> value` returns the selected tonic as a MIDI note, and `scale() -> value` returns 0 for major, 1 for natural minor, or 2 for pentatonic.
- `emit(track, note) -> void` stores a note in the host composition; `-1` means rest.

`theory.ks` exports `pitch(degree) -> value`, which maps scale degrees using the host's root and scale controls. Each of `bass.ks`, `drums.ks`, and `lead.ks` imports the shared theory module and exports `pattern(step) -> value`. The root wraps those patterns in the `bass_note`, `drums_note`, and `lead_note` entries. Scripts describe pitch theory and musical patterns; the host owns note storage, editing, track settings, synthesis, and file formats. Edit the `.ks` modules in `music/` and use **Reload Scripts** to install a changed arrangement. Piano-roll edits are preserved by script reloads and regional generation.

A useful customization is to have `lead.ks` return `-1` on alternate bars before
choosing its chord tones, creating a call-and-response phrase. Or change which
steps return the percussion pitches in `drums.ks` while leaving scale mapping
in the shared theory module.

## Documents and exports

Cadence documents store bar count, tempo, key root, scale, mute and instrument settings, and all 128 sixteenth-note positions per track. Notes in bars hidden by a shorter visible song length remain in the document and return when you lengthen it. The WAV exporter writes deterministic mono, 16-bit PCM at 22050 Hz. Each track can use a sine, harmonic, or modulated tone. Playback is optional; export the WAV and open it in a player of your choice.

For a larger composition, generate an eight-bar backing loop, edit a few bass
notes by hand, then regenerate only lead steps 65 through 128. Save the complete
arrangement and export WAV. This exercises 384 stored track positions, shared
theory imports, regional generation and host synthesis in one usable project.

## Verification

The executable's `--verify --test-root <directory>` path is headless. It generates an eight-bar arrangement, checks note bounds and volume, confirms root and scale changes affect generated notes, verifies all-128-step document roundtrip and overwrite, checks regional edit preservation, compares deterministic WAV output as tempo/instrument/mute changes, and exercises valid and failed script reloads plus runtime, missing-entry, wrong-track, duplicate-emission, and all-track generation failures. CTest runs this same path under `build/test-tmp/Cadence`.
