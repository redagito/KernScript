// ScriptDash alerts: threshold policy with host-held hysteresis flags.
//
// The armed flag lives host-side per chart; this module only pulls it
// through the valued extern is_armed and emits the new state through the
// statement extern set_armed (each extern is used consistently one way).
// check fires once (returns 1) when value crosses limit while armed, then
// disarms; it re-arms only after value falls below limit minus the
// hysteresis band, so a sustained over-limit plateau alerts exactly once.
// limit_for keeps the per-chart thresholds in script so Reload applies
// policy edits live. All valued: every return carries an expression.
module alerts;
import stats;
extern is_armed(chart);
extern set_armed(chart, flag);
export func limit_for(chart) {
  var c = stats::clamp(chart, 0, 2);
  if (c == 0) {
    return 8.0;
  }
  if (c == 1) {
    return 12.0;
  }
  return 6.0;
}
export func check(chart, value, limit) {
  var a = is_armed(chart);
  if (value > limit) {
    if (a == 1) {
      set_armed(chart, 0);
      return 1;
    }
    return 0;
  }
  var band = limit - 2.0;
  if (value < band) {
    set_armed(chart, 1);
    return 0;
  }
  return 0;
}
export func check_src(src, chart) {
  var w = stats::alert_window();
  var a = stats::avg(src, w);
  var lim = limit_for(chart);
  var c = check(chart, a, lim);
  return c;
}
