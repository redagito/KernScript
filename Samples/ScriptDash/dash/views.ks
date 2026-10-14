// ScriptDash views: which source feeds which chart.
//
// The host view-mode flag (0/1/2, set by the View A/B/C buttons) is
// pulled through the valued extern view_mode; the mapping itself is pure
// script policy. stats::clamp keeps the chart index honest, which is the
// genuine use behind the stats import (diamond leaf, like alerts).
module views;
import stats;
extern view_mode();
export func source_for_chart(chart) {
  var m = view_mode();
  var c = stats::clamp(chart, 0, 2);
  if (m == 1) {
    if (c == 0) {
      return 2;
    }
    if (c == 1) {
      return 3;
    }
    return 0;
  }
  if (m == 2) {
    if (c == 0) {
      return 3;
    }
    if (c == 1) {
      return 0;
    }
    return 1;
  }
  return c;
}
