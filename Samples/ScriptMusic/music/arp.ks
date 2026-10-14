// ScriptMusic arp: chord arpeggio, one octave above the bass roots.
//
// Imports theory (diamond leaf). pattern(step) is valued: root, third,
// fifth, then the octave (preset 0) or the high fifth (preset 1) on the
// bar tail. All chord tones derive from theory so the hub stays shared.
module arp;
import theory;
extern preset();
export func pattern(step) {
  var bar = step / 4;
  var root = theory::chord_root(bar) + 12;
  var third = root + theory::chord_third(bar);
  var top = theory::fifth(root);
  var pos = step - bar * 4;
  if (pos == 0) {
    return root;
  }
  if (pos == 1) {
    return third;
  }
  if (pos == 2) {
    return top;
  }
  if (preset() == 1) {
    return theory::octave_up(top);
  }
  return theory::octave_up(root);
}
