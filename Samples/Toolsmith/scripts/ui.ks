// Text Formatter tool. The host supplies named fields, text operations and
// generic document storage; this script owns the visible controls and actions.
extern tool(name, script) -> void;
extern field(name, label, multiline) -> void;
extern button(name, label, handler) -> void;
extern get_text(name) -> value;
extern set_text(name, text) -> void;
extern upper(text) -> value;
extern lower(text) -> value;
extern trim(text) -> value;
extern normalize(text) -> value;
extern replace(text, find, replacement) -> value;
func build() -> void {
  tool("Text Formatter", "ui.ks");
  field("input", "Input text", "yes");
  field("find", "Find (required for Replace)", "no");
  field("replacement", "Replace with", "no");
  field("output", "Output", "output");
  button("uppercase", "UPPERCASE", "on_upper");
  button("lowercase", "lowercase", "on_lower");
  button("trim", "Trim edges", "on_trim");
  button("normalize", "Normalize spaces", "on_normalize");
  button("replace", "Replace text", "on_replace");
  button("clear", "Clear", "on_clear");
}
func on_upper() -> void { set_text("output", upper(get_text("input"))); }
func on_lower() -> void { set_text("output", lower(get_text("input"))); }
func on_trim() -> void { set_text("output", trim(get_text("input"))); }
func on_normalize() -> void { set_text("output", normalize(get_text("input"))); }
func on_replace() -> void {
  var find = get_text("find");
  if (find == "") { set_text("output", "Enter text to find before replacing."); }
  else { set_text("output", replace(get_text("input"), find, get_text("replacement"))); }
}
func on_clear() -> void {
  set_text("input", ""); set_text("find", ""); set_text("replacement", ""); set_text("output", "");
}
func verify_late_failure() -> void {
  set_text("output", "partial change");
  var invalid = replace("text", "", "x");
}
