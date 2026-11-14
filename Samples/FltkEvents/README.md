# FltkEvents

FltkEvents is the portable counterpart of
[NativeEvents](../NativeEvents/README.md). Enable GUI samples, build the
`FltkEvents` target, and launch it. FLTK provides the window and buttons.

Click **Click me** to increment the count. The script classifies the first
click and each ten-click milestone, and **Reset** restores the initial state.
The host displays the count and prints the event classification to the console.

C++ owns the count and widgets and exposes `get_count() -> value` and
`emit(code, next) -> void`. Button callbacks invoke the zero-argument script
entries `on_click()` and `on_reset()`. One VM remains loaded across callbacks.

Change the milestone interval in the embedded source to five and rebuild to
customize the behavior. Comparing this file with NativeEvents shows that the
same script/host protocol fits both FLTK callbacks and native Windows messages.

This is a focused event-binding tutorial. For a useful application with more
data and more frequent script calls, try [Daybook](../Daybook/README.md) or
[Gridbook](../Gridbook/README.md). Their headless verifiers provide larger
application workloads. The integration suite compiles this tutorial's script
and exercises clicks, milestones and reset without opening a window. The FLTK
button event routing can be checked manually when convenient.
