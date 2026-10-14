// ScriptMusic bass: root/fifth/walk line over a 4-bar loop.
//
// Imports theory (diamond leaf). pattern(step) is valued: one MIDI-ish
// int per 16th-step, 0-15. Bar roots come from theory::chord_root; the
// C-rooted bar drops an octave via a string == check on
// theory::note_name (string-table + dispatch pressure, deterministic).
// preset() is a host-valued extern: 0 plays the plain line, 1 pops the
// backbeat up an octave and walks degrees on the off-beats.
module bass;
import theory;
extern preset();
export func pattern(step) {
  var bar = step / 4;
  var root = theory::chord_root(bar);
  if (theory::note_name(root) == "C") {
    root = root - 12;
  }
  var pos = step - bar * 4;
  if (pos == 0) {
    return root;
  }
  if (pos == 2) {
    return theory::fifth(root);
  }
  if (preset() == 1) {
    if (pos == 3) {
      return theory::octave_up(root);
    }
    return root + theory::deg(2);
  }
  if (pos == 3) {
    return root + theory::deg(1);
  }
  return root;
}
