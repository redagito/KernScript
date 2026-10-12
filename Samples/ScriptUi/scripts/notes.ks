// Swap Pad brain: a SECOND ScriptUi script with a different shape and
// different behavior, proving the host shell assumes nothing about the
// script it runs. One label, two fields, two buttons (ui.ks has three
// buttons and an extra field flow); handlers swap two texts instead of
// uppercasing one.
//
// Same host protocol as ui.ks (see its header): the host calls build()
// first, lays widgets out in creation order, and --verify presses the REAL
// buttons in that order, calling verify_setup() once beforehand and
// check_<handler> after each press. All names below are this script's own.
extern create_label(name, text);
extern create_field(name);
extern create_button(name, caption);
extern on_click(name, handler);
extern get_text(name);
extern set_text(name, text);
extern upper(text);
extern set_title(text);
extern check_equal(actual, expected);
extern quit();
func build() {
  set_title("Swap Pad Demo");
  create_label("pad_title", "Two-field swap pad: Swap exchanges sides");
  create_field("left");
  create_field("right");
  create_button("swap", "Swap");
  create_button("bye", "Bye");
  on_click("swap", "on_swap");
  on_click("bye", "on_bye");
}
func on_swap() {
  var a = get_text("left");
  var b = get_text("right");
  set_text("left", b);
  set_text("right", a);
}
func on_bye() {
  quit();
}
func verify_setup() {
  set_text("left", "abc");
  set_text("right", "123");
}
func check_on_swap() {
  var a = get_text("left");
  var b = get_text("right");
  check_equal(a, "123");
  check_equal(b, "abc");
}
