// The root owns story identity and delegates scene content to chapter modules.
import chapter1;
import chapter2;
extern node() -> value;
extern story_id(id) -> void;
func render() -> void {
  story_id("ember-road-v1");
  var n = node();
  if (chapter1::play(n) == 1) { return; }
  chapter2::play(n);
}
