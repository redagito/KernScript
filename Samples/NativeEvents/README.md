# NativeEvents

NativeEvents demonstrates connecting Windows GUI events to a persistent KernScript VM. Build the `NativeEvents` target with command-line samples enabled and run it on Windows. **Click me** increments the count; **Reset** clears it.

The host owns the window and count. It exposes `get_count() -> value` and `emit(code, next) -> void`, then calls `on_click() -> void` or `on_reset() -> void` from native event handlers. The script reports the first click and each ten-click milestone. Change the milestone interval in the embedded script in `source/main.cpp` and rebuild. The VM is compiled once and reused across events.

See [FltkEvents](../FltkEvents/README.md) for the portable FLTK version.
