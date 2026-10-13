// ScriptPaint geometry library: bounds helpers for the paint tools.
// Single-return style throughout (valued funcs, no bare returns).
module geom;
export func clamp(v, lo, hi) {
  var out = v;
  if (v < lo) {
    out = lo;
  } else {
    if (v > hi) {
      out = hi;
    }
  }
  return out;
}
export func inside(x, y, w, h) {
  var ok = 1;
  if (x < 0) {
    ok = 0;
  }
  if (y < 0) {
    ok = 0;
  }
  if (x > w - 1) {
    ok = 0;
  }
  if (y > h - 1) {
    ok = 0;
  }
  return ok;
}
