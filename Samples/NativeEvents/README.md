# NativeEvents

NativeEvents is a small Windows tutorial for connecting GUI events to a
persistent KernScript VM. Build the `NativeEvents` target with command-line
samples enabled and launch it on Windows.

Click **Click me** to increment the displayed count and **Reset** to return
to the initial state. The script reports a first-click event and a milestone
every ten clicks. C++ translates those result codes into the label and console
messages.

The host owns the window and count. It exposes `get_count() -> value` and
`emit(code, next) -> void`, then calls `on_click() -> void` or
`on_reset() -> void` from native event handlers. Scripts decide the next count
and event classification. The VM is compiled and loaded once and reused across
events.

For a useful customization, change the milestone interval from ten to five in
the embedded script and rebuild. The condition uses integer arithmetic:
`next / 5 * 5 == next`. The native window code needs no changes.

[FltkEvents](../FltkEvents/README.md) implements the same behavior with a
portable UI toolkit, making the platform-specific integration easy to compare.
These two projects stay small so the event bridge is visible. The larger apps
demonstrate document storage, external modules, reloads, and realistic workloads.
The integration suite compiles their embedded scripts and exercises 25 clicks,
milestone reporting, reset and the next first click without opening windows.
The native button event routing can be checked manually when convenient.
