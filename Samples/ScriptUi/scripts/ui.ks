// ScriptUi brain: the SCRIPT builds the whole UI from a bare host shell.
//
// Host protocol (tiny, fixed):
//   create_label(name, text) statement-use: registers a label under name.
//   create_field(name) statement-use: registers a text field under name.
//   create_button(name, caption) statement-use: registers a button under name.
//   on_click(name, handler) statement-use: attaches a script func BY NAME.
//   get_text(name) value-use: reads a field by name.
//   set_text(name, text) statement-use: writes a field by name.
//   upper(text) value-use: ASCII uppercase transform.
//   quit() statement-use: ends the app.
//
// Layout is creation order, vertical stack (host-side). Widgets are known
// by name everywhere: handlers use the same names build() registered, so
// no address book of integer handles is needed. Duplicate creation names
// and unknown names in get/set/on_click fail the run loudly.
extern create_label(name, text);
extern create_field(name);
extern create_button(name, caption);
extern on_click(name, handler);
extern get_text(name);
extern set_text(name, text);
extern upper(text);
extern quit();
func build() {
  create_label("title", "Enter text, Transform copies UPPER case");
  create_field("input");
  create_field("output");
  create_button("close", "Close");
  create_button("transform", "Transform");
  create_button("clear", "Clear");
  on_click("close", "on_close");
  on_click("transform", "on_transform");
  on_click("clear", "on_clear");
}
func on_transform() {
  var t = get_text("input");
  var u = upper(t);
  set_text("output", u);
}
func on_clear() {
  set_text("input", "");
  set_text("output", "");
}
func on_close() {
  quit();
}
