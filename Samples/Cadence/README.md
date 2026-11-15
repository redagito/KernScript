# Cadence

Cadence is a FLTK music workstation. The host owns the composition, editor, playback and WAV export; KernScript modules generate bass, percussion and lead notes.

## Use

Build and run the `Cadence` target with GUI samples enabled. Click piano-roll cells to toggle notes. The arrangement holds eight bars (128 sixteenth-note steps) per track; the visible length can be two, four or eight bars. Set an inclusive step range and use **Generate Track** or **Generate All** to replace only those notes. **Reload Scripts** validates a candidate arrangement before switching. New, Open, Save and Save As manage documents; unsaved edits are checked before replacement or close. BPM is 40–240. Root and scale affect later generation, not existing notes.

Documents store bar count, tempo, root, scale, mute and instrument settings, plus all 128 steps per track. Hidden notes remain saved when the visible length is shortened. WAV export is mono, 16-bit PCM at 22050 Hz.

## Script API

The root script imports `bass`, `drums` and `lead` from `music/`. The host provides:

- `cur_step() -> value`: current sixteenth-note index.
- `root() -> value`: selected tonic as a MIDI note; `scale() -> value`: 0 major, 1 natural minor, 2 pentatonic.
- `emit(track, note) -> void`: store a note; `-1` is a rest.

`theory.ks` exports `pitch(degree) -> value`. Each track module exports `pattern(step) -> value`; the root connects these to `bass_note`, `drums_note` and `lead_note`. Edit the `.ks` files under `music/` and reload. For example, return `-1` on alternate bars in `lead.ks` to create a call-and-response phrase.

`Cadence --verify --test-root <directory>` runs the headless verifier; CTest registers `Cadence.Verify`.
