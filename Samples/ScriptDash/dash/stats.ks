// ScriptDash stats: window math over host-held ring buffers.
//
// The host owns every sample (ring buffer per source); scripts keep no
// state across calls. Each function pulls its window sample-by-sample
// through the valued externs src_count/src_val, so every extern call is
// hoisted into a temp var (never nested inside an operator, which the
// compiler rejects). All functions are valued: every return carries an
// expression. No string arithmetic anywhere: only ints and floats flow.
module stats;
extern src_count(src);
extern src_val(src, idx);
export func clamp(x, lo, hi) {
  if (x < lo) {
    return lo;
  }
  if (x > hi) {
    return hi;
  }
  return x;
}
export func disp_window() {
  return 24;
}
export func alert_window() {
  return 6;
}
export func avg(src, want) {
  var c = src_count(src);
  var n = want;
  if (n > c) {
    n = c;
  }
  if (n <= 0) {
    return 0.0;
  }
  var start = c - n;
  var i = 0;
  var sum = 0.0;
  while (i < n) {
    var k = start + i;
    var v = src_val(src, k);
    sum = sum + v;
    i = i + 1;
  }
  return sum / n;
}
export func min_of(src, want) {
  var c = src_count(src);
  var n = want;
  if (n > c) {
    n = c;
  }
  if (n <= 0) {
    return 0.0;
  }
  var start = c - n;
  var best = src_val(src, start);
  var i = 1;
  while (i < n) {
    var k = start + i;
    var v = src_val(src, k);
    if (v < best) {
      best = v;
    }
    i = i + 1;
  }
  return best;
}
export func max_of(src, want) {
  var c = src_count(src);
  var n = want;
  if (n > c) {
    n = c;
  }
  if (n <= 0) {
    return 0.0;
  }
  var start = c - n;
  var best = src_val(src, start);
  var i = 1;
  while (i < n) {
    var k = start + i;
    var v = src_val(src, k);
    if (v > best) {
      best = v;
    }
    i = i + 1;
  }
  return best;
}
