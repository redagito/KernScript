// ScriptMusic theory: shared scale/chord/note hub (the diamond hub).
//
// bass, drums and arp all import theory, and the host root imports all
// three voices, so every compile loads theory once and shares it.
// All funcs are valued (return expr only, no bare returns). Pitch math
// stays host-side (frequency from MIDI); scripts work in scale degrees
// and MIDI-ish ints only. No string arithmetic anywhere: note names flow
// out as values and callers compare them with ==/!=.
module theory;
export func deg(d) {
  if (d == 0) {
    return 0;
  }
  if (d == 1) {
    return 2;
  }
  if (d == 2) {
    return 3;
  }
  if (d == 3) {
    return 5;
  }
  if (d == 4) {
    return 7;
  }
  if (d == 5) {
    return 8;
  }
  if (d == 6) {
    return 10;
  }
  return 12;
}
export func chord_root(bar) {
  if (bar == 0) {
    return 45;
  }
  if (bar == 1) {
    return 41;
  }
  if (bar == 2) {
    return 48;
  }
  return 43;
}
export func chord_third(bar) {
  if (bar == 0) {
    return 3;
  }
  if (bar == 1) {
    return 4;
  }
  if (bar == 2) {
    return 4;
  }
  return 4;
}
export func note_name(n) {
  var p = n - (n / 12) * 12;
  if (p == 0) {
    return "C";
  }
  if (p == 1) {
    return "C#";
  }
  if (p == 2) {
    return "D";
  }
  if (p == 3) {
    return "D#";
  }
  if (p == 4) {
    return "E";
  }
  if (p == 5) {
    return "F";
  }
  if (p == 6) {
    return "F#";
  }
  if (p == 7) {
    return "G";
  }
  if (p == 8) {
    return "G#";
  }
  if (p == 9) {
    return "A";
  }
  if (p == 10) {
    return "A#";
  }
  return "B";
}
export func fifth(n) {
  return n + 7;
}
export func octave_up(n) {
  return n + 12;
}
