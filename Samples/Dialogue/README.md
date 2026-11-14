# Dialogue

Dialogue is a conversation authoring playground. Load a KernScript scenario,
send messages, inspect the matched rule and current state, and replay a saved
conversation after editing its rules. Two complete examples show customer
support routing and an NPC with inventory and progression.

## Try the scenarios

Start `Dialogue` with the GUI samples built. **Shop Support** is selected by
default. Send `delivery`, then `late`, then `arrived`. The same conversation
remembers the topic and follow-up status. Try `returns`, followed by `damaged`
or `unwanted`. Use `help` for available topics or `reset` to clear the flow.

Choose **Gatekeeper NPC** for a small playable dialogue. Try `enter`, `buy`,
`enter`, `bridge`, `lantern`, then `bridge`. The state inspector shows coins,
the permit and progress through the gate. Repeating `buy` does not charge twice
for a permit you already own. Each conversation starts with its own state.

The inspector shows which named rule produced the latest reply. **Undo Message**
returns to the state before the last message, so authors can explore alternatives.
**New** restarts the current scenario. Switching scenarios starts a new conversation.

**Save**, **Open** and **Save As** preserve a conversation as a readable, versioned
`.dialogue` file. **Export Transcript** writes the messages, replies and matched
rules as text. Before replacing a conversation or closing, the app handles a
pending draft and offers to save sent messages. Save and transcript export also
handle a draft explicitly. Existing output replacement is confirmed, and a
transcript cannot overwrite the active saved conversation or root scenario.

```text
Dialogue --scenario scripts/gatekeeper.ks
Dialogue --scenario scripts/gatekeeper.ks --open gate.dialogue
Dialogue --help
```

Runtime scripts are copied beside the executable. Source-tree runs also search
`scripts/` and `Samples/Dialogue/scripts/`. Build the `Dialogue` target through
the project's normal Conan/CMake setup with GUI samples enabled.

## Author a conversation

Copy a bundled scenario and its `common.ks` helper, edit it, then choose
**Load Scenario**. Each scenario must expose `func respond() -> void`.
The host calls it once with an empty input for the welcome message, then once
per sent message. State updates and the reply commit together when the call
succeeds. No script state is retained outside the named host state.

| Host function | Purpose |
| --- | --- |
| `input()` | Current input string; empty for the welcome call |
| `has_word(word)` | ASCII case-insensitive match at whole-word boundaries |
| `turn_number()` | Zero for the welcome call, then one per user message |
| `state(name)` | Named integer state, zero if unset |
| `set_state(name, value)` | Set a value in this conversation |
| `scenario(id)` | Required once per response; stable scenario identity |
| `reply(rule_id, text)` | Required exactly once; emit the matched rule and reply |

Scenario, rule and state IDs use letters, digits, dots, underscores and hyphens,
up to 80 bytes. Inputs are limited to 4096 bytes, replies to 8192 bytes, and a
conversation to 1000 messages, 128 state values and an 8 MiB text history.
Each response has a 100000-instruction budget. Compiler and runtime errors are
reported without appending partial replies or changing conversation state.

A useful customization is to add a support topic for replacement parts. Set a
new `topic` value when the user asks about parts, then match a follow-up word
such as `missing` or `broken`. Give each branch a distinct rule ID so authors
can see why a reply matched. Keep a fallback rule and add the new topic to help.

## Replaying and reloading

Saved files contain the scenario identity and input journal. Opening one replays
the inputs through the loaded scenario to reconstruct replies and state. Choose
the right scenario before opening its saved conversations. **Reload Script**
compiles a candidate and replays the current conversation before replacing the
working VM. Authors can see how their revised rules respond to an existing
conversation. Failed compilation, mismatched scenario identity or failed replies
leave the previous program and conversation available. Replaying starts from
fresh state, so entry effects are not applied twice.

For a larger authoring project, build a support routing flow with several topics
or an NPC quest with inventory checks, alternate paths and distinct completion
rules. Keep representative conversations as regression inputs while changing
scripts. The headless verifier saves, opens and reloads a 600-message session
with state updates, alongside the complete support and gatekeeper workflows.

`Dialogue --verify --test-root build/test-tmp/Dialogue` runs without windows.
CTest registers `Dialogue.Verify`. It covers state isolation, matched rules,
whole-word behavior, undo, saved replay, transcript output, invalid saves,
failed reloads, partial-response rollback and the larger authoring session.
