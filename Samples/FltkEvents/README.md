# FltkEvents

FltkEvents demonstrates a persistent KernScript VM called from FLTK button callbacks. Enable GUI samples, build the `FltkEvents` target and run it. **Click me** increments the count; **Reset** sets it to zero.

The host owns the widgets and count. It exposes `get_count() -> value` and `emit(code, next) -> void`; callbacks invoke `on_click() -> void` and `on_reset() -> void`. The script classifies the first click and each ten-click milestone. To change the interval, edit the embedded script in `source/main.cpp` and rebuild.

See [NativeEvents](../NativeEvents/README.md) for the equivalent Windows message integration.
