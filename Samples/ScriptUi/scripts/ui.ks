// ScriptUi brain: the SCRIPT builds the whole UI from a bare host shell.
//
// Host protocol (tiny, fixed):
//   create_label(text) value-use: returns an integer handle (1-based).
//   create_field() value-use: returns an integer handle for a text field.
//   create_button(caption) value-use: returns an integer handle for a button.
//   on_click(button, handler) statement-use: attaches a script func BY NAME.
//   get_text(field) value-use: reads a field.
//   set_text(field, text) statement-use: writes a field.
//   upper(text) value-use: ASCII uppercase transform.
//   quit() statement-use: ends the app.
//
// Layout is creation order, vertical stack (host-side). Handles are
// sequential in creation order starting at 1, so this script runs:
//   1 = label, 2 = first field, 3 = second field,
//   4 = Close, 5 = Transform, 6 = Clear.
// Handlers below use those well-known ids because the language has no
// globals: build() locals cannot be seen from other funcs, so the order
// doubles as the shared address book. No string arithmetic anywhere;
// handler names are plain strings dispatched host-side.
extern create_label(text);
extern create_field();
extern create_button(caption);
extern on_click(button, handler);
extern get_text(field);
extern set_text(field, text);
extern upper(text);
extern quit();
func build() {
  var title = create_label("Enter text, Transform copies UPPER case");
  var f1 = create_field();
  var f2 = create_field();
  var closeBtn = create_button("Close");
  var transBtn = create_button("Transform");
  var clearBtn = create_button("Clear");
  on_click(closeBtn, "on_close");
  on_click(transBtn, "on_transform");
  on_click(clearBtn, "on_clear");
}
func on_transform() {
  var t = get_text(2);
  var u = upper(t);
  set_text(3, u);
}
func on_clear() {
  set_text(2, "");
  set_text(3, "");
}
func on_close() {
  quit();
}
