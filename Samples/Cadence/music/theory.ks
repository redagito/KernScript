// Shared host-controlled scale theory used by each arrangement module.
// root() is the tonic MIDI note; scale() is 0 major, 1 natural minor, 2 pentatonic.
module theory;
extern root() -> value;
extern scale() -> value;
export func pitch(degree) -> value {
  var count = 7;
  if (scale() == 2) {
    count = 5;
  }
  var octave = degree / count;
  var index = degree - octave * count;
  if (index < 0) {
    index = index + count;
    octave = octave - 1;
  }
  var interval = 0;
  if (scale() == 2) {
    if (index == 1) { interval = 3; }
    if (index == 2) { interval = 5; }
    if (index == 3) { interval = 7; }
    if (index == 4) { interval = 10; }
  } else {
    if (index == 1) { interval = 2; }
    if (index == 2) {
      interval = 4;
      if (scale() == 1) { interval = 3; }
    }
    if (index == 3) { interval = 5; }
    if (index == 4) { interval = 7; }
    if (index == 5) {
      interval = 9;
      if (scale() == 1) { interval = 8; }
    }
    if (index == 6) {
      interval = 11;
      if (scale() == 1) { interval = 10; }
    }
  }
  return root() + octave * 12 + interval;
}
