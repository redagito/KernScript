// Bassline generator. Host pitch(degree) maps these scale degrees through
// the selected root and scale. Change the branches to customize the groove.
module bass;
import theory;
export func pattern(step) -> value {
  var beat = step - (step / 4) * 4;
  var bar = step / 16;
  if (beat == 0) { return theory::pitch(-7 + bar); }
  if (beat == 2) { return theory::pitch(-3 + bar); }
  if (beat == 3) { return theory::pitch(-2 + bar); }
  return -1;
}
