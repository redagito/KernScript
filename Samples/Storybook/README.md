# Storybook

Storybook is a branching story player and authoring host. The host owns saved playthroughs, history and named state; KernScript modules define scenes, choices, effects and endings.

## Play and saves

Build `Storybook` with GUI samples enabled. Start a new traveler to play the bundled story. **Continue** advances ordinary scenes; choices must be selected. **Back / Undo** reverses the latest transition, including state changes. `Storybook --open journey.storybook` opens a save; `--story chapters/story.ks` selects a story; `--help` lists options.

Versioned `.storybook` saves contain traveler, story identity and a transition journal. Opening replays the journal through the selected story to reconstruct dialogue and flags. Choose the matching story before opening a save. Reload compiles and replays the current journal before switching scripts; changed or invalid routes retain the current game. Transcript export writes readable text.

## Author scenes

Copy `chapters/` and use **Load Story Script** to select `story.ks`. The root imports chapter modules; `start` is the initial scene ID. Define `func render() -> void` to describe the current scene and update named integer flags. Updates commit with the scene on entry.

| Host function | Purpose |
| --- | --- |
| `node()`, `hero()` | Current scene ID and traveler name |
| `get_flag(name)`, `set_flag(name, value)` | Read or set named integer state |
| `story_id(id)` | Required once per scene; save compatibility identity |
| `title(text)`, `say(actor, text)` | Set title and append dialogue |
| `choice(id, label, target)` | Add one of up to three choices with distinct IDs |
| `next(target)` | Offer a destination for Continue |
| `finish(ending)` | End the playthrough |

Declare `node`, `hero` and `get_flag` as `-> value`; other bindings return `void`.
Each scene needs one story ID, one title, dialogue and exactly one exit type:
choices, `next`, or `finish`.

IDs allow letters, digits, underscores, dots and hyphens. Limits: 100,000
instructions per scene, 200 dialogue lines, 64 KiB output, 256 flags, 1,000
transitions and 8 MiB backlog.

To add a branch, add a choice with a new target scene, set a flag there, and
read it with `get_flag` in a later scene. Keep IDs stable for save compatibility.

`Storybook --verify --test-root <directory>` runs headless checks; CTest registers `Storybook.Verify`.
