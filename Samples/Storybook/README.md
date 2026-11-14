# Storybook

Storybook is a playable branching story and a small authoring host. The host
owns saved playthroughs, history and named state. KernScript modules own scene
content, routes, available choices, entry effects and endings.

## Play a story

Start `Storybook`, enter a name for a new traveler and press **New**. The bundled
story, *Ember Gate and the Hollow Road*, has 26 scenes and five final endings.
Help Mara, choose supplies from Bram, and decide whether to trust Wren. These
choices change later scenes and carry into the second chapter. An iron charm
can offer a second chance at the Hollow; helping at the gate changes the home
you can return to.

Use **Continue** between ordinary scenes. At a choice, select an offered action;
Continue cannot skip it. The scene history lets you reread earlier dialogue
and see the choice you made there. **Current** returns to the active scene.
**Back / Undo** reverses the most recent transition, including that scene's state
changes, so you can try another branch.

**Save** and **Open** preserve the traveler and every selected route. **Export
Transcript** produces a readable text copy of the scenes, choices and ending.
New, Open, loading another story and closing the window prompt about unsaved
progress. Save As and transcript export confirm replacement of existing files.
The active saved game and root story script cannot be overwritten by transcript
export. A failed load, script reload or transition retains the current game.

Command-line shortcuts:

```text
Storybook --open journey.storybook
Storybook --story chapters/story.ks
Storybook --help
```

The executable looks for its bundled `chapters/` directory beside itself, then
under the current directory or `Samples/Storybook/`. Use the project's normal
Conan/CMake setup with GUI samples enabled to build the `Storybook` target.

## Author scenes

Copy the `chapters/` directory into your project and use **Load Story Script** to
select its `story.ks`. The root imports chapter modules. Both chapters import
the shared cast and world modules, exercising a real shared import graph.
There is no fixed chapter cursor in the host. Scene IDs and routes live in
scripts. The initial scene ID is `start`.

The root must expose `func render() -> void`. Each call describes one scene
and may update the host's named integer flags. These updates happen once when
entering a scene. The host commits the complete scene and its flags together.

| Host function | Purpose |
| --- | --- |
| `node()` | Current scene ID string |
| `hero()` | Traveler name string |
| `get_flag(name)` | Current integer value, or zero when unset |
| `set_flag(name, value)` | Set an integer story variable |
| `story_id(id)` | Required once per scene; stable identity for compatible saves |
| `title(text)` | Required once per scene |
| `say(actor, text)` | Append a dialogue line |
| `choice(id, label, target)` | Add one of up to three choices with distinct IDs |
| `next(target)` | Offer Continue to another scene |
| `finish(ending)` | End the playthrough with a named ending |

Every scene needs dialogue and exactly one exit type: a choice group, a next
destination, or an ending. IDs use letters, digits, underscores, dots and
hyphens. An undefined destination fails without advancing. Scripts cannot
modify the UI directly. A scene has a 100000-instruction budget, 200 dialogue
lines and 64 KiB of output. Stories support 256 named flags; playthroughs support
1000 transitions and an 8 MiB backlog.

A worthwhile first customization is to add a **keep watch** route at the gate.
Give it a new scene ID, set a `kept_watch` flag there, and use `get_flag` in the
Hollow chapter to reveal a warning the traveler overheard. Keep old scene and
choice IDs when you want existing saves to remain compatible.

## Saves and script reloads

Readable, versioned `.storybook` files store the traveler, story identity, and
transition journal. Opening a save replays that journal using the loaded story.
This reconstructs dialogue and flags instead of trusting arbitrary saved
state. **Reload Story Scripts** compiles a candidate and replays the current
journal before replacing the working program. Entry effects and dialogue are
reconstructed from the beginning, without being appended twice. Text changes
appear in the refreshed history. If a recorded choice or destination has
changed, reload or Open reports the incompatibility and keeps the current game.
Choose the right story script before opening saves for a custom story.

For a larger project, split a mystery into location modules with persistent
clues, witnesses, alternate routes and multiple endings. The included headless
verifier also runs a 121-scene archive with 2420 dialogue lines and repeated
state updates, then saves, opens and reloads the complete playthrough.

## Verify

`Storybook --verify --test-root build/test-tmp/Storybook` runs without windows.
CTest registers it as `Storybook.Verify`. It traverses every reachable bundled
scene/state combination and checks all five endings, required choices, cross
chapter state, undo, save/open/overwrite, transcript output, invalid saves,
failed compilation, failed scene execution, protocol errors and the larger
archive workflow.
