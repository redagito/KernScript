// Percussion lane: MIDI drum pitches remain fixed while pattern timing is
// customizable. -1 is a rest.
module drums;
import theory;
export func pattern(step) -> value {
  var beat = step - (step / 4) * 4;
  if (beat == 0) { return 36; }
  if (beat == 2) {
    if (step == 4) { return 40; }
    if (step == 12) { return 40; }
    return 44;
  }
  if (beat == 1) { return 44; }
  return -1;
}
