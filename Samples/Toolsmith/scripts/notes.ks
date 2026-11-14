// Notes tool. Saving and opening versioned .kstool documents is generic host
// behavior; title and body are ordinary named fields owned by this script.
extern tool(name, script) -> void;
extern field(name, label, multiline) -> void;
extern button(name, label, handler) -> void;
extern get_text(name) -> value;
extern set_text(name, text) -> void;
func build() -> void {
  tool("Notes", "notes.ks");
  field("title", "Title", "no");
  field("body", "Note", "yes");
  button("new_note", "Clear note", "on_new");
}
func on_new() -> void { set_text("title", ""); set_text("body", ""); }
func verify_late_failure() -> void {
  set_text("title", "partial change");
  set_text("unknown_field", "failure");
}
