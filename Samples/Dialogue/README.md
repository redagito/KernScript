# Dialogue

Dialogue is a scenario authoring host for scripted conversations. The host owns the conversation, named state, undo and files; KernScript defines rules and replies. Bundled examples cover support routing and an NPC quest.

## Run

Build the `Dialogue` target with GUI samples enabled. The Shop Support scenario starts selected. Choose **Gatekeeper NPC** for the second example. `Dialogue --scenario scripts/gatekeeper.ks` selects a scenario; add `--open gate.dialogue` to replay a saved conversation. `--help` lists options. Scripts are copied beside the executable; source-tree runs also search `scripts/` and `Samples/Dialogue/scripts/`.

Conversations can be saved as versioned `.dialogue` files or exported as transcripts. Saved files contain the scenario identity and input journal; opening replays inputs using the selected scenario to reconstruct replies and state. Choose the matching scenario before opening a save. Reload compiles and replays the current conversation before replacing the active VM. Failed loads and replies preserve the current conversation. Inputs are limited to 4096 bytes, replies to 8192 bytes, and conversations to 1000 messages, 128 state values and 8 MiB of text history.

## Scenario API

Copy a scenario and its `common.ks` helper, then use **Load Scenario**. Each scenario exports `func respond() -> void`. The host calls it once with empty input for the welcome message, then once per sent message. State and reply changes commit together on success. Bindings:

| Function | Purpose |
| --- | --- |
| `input()` | Current input; empty for welcome |
| `has_word(word)` | ASCII case-insensitive whole-word match |
| `turn_number()` | Zero for welcome, then one per user message |
| `state(name)`, `set_state(name, value)` | Read or set named integer state |
| `scenario(id)` | Required once per response; stable scenario identity |
| `reply(rule_id, text)` | Required exactly once; records matched rule and reply |

Declare readers as `-> value`; `set_state`, `scenario` and `reply` return `void`.

Scenario, rule and state IDs use letters, digits, dots, underscores and hyphens, up to 80 bytes. Each response has a 100,000-instruction budget. Add a support topic by setting a topic state, handling a follow-up input with a distinct rule ID, and adding the topic to the help response.

`Dialogue --verify --test-root <directory>` runs headless checks; CTest registers `Dialogue.Verify`.
