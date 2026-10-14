// ScriptMusic drums: kick/snare/hat groove as pitched cells.
//
// Imports theory (diamond leaf) and voices theory::deg on the hats so
// the import is a real use, not a formality. pattern(step) is valued:
// 36 kick, 40 snare, 44 hat, -1 rest. Snare replaces the kick on steps
// 4 and 12; preset 1 doubles the kick onto every pos-2 off-beat.
module drums;
import theory;
extern preset();
export func pattern(step) {
  if (step == 4) {
    return 40;
  }
  if (step == 12) {
    return 40;
  }
  var bar = step / 4;
  var pos = step - bar * 4;
  if (pos == 0) {
    return 36;
  }
  if (pos == 2) {
    if (preset() == 1) {
      return 36;
    }
    return 44;
  }
  if (pos == 1) {
    return 44 + theory::deg(0);
  }
  return -1;
}
