// Chord-tone melody generator. The shared theory module applies the selected
// scale, so the same arrangement adapts when root or scale changes.
module lead;
import theory;
export func pattern(step) -> value {
  var pos = step - (step / 8) * 8;
  var bar = step / 16;
  if (pos == 0) { return theory::pitch(bar); }
  if (pos == 2) { return theory::pitch(bar + 2); }
  if (pos == 4) { return theory::pitch(bar + 4); }
  if (pos == 6) { return theory::pitch(bar + 6); }
  return -1;
}
