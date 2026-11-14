// Host policy for a one-sided alert with a hysteresis re-arm band.
module alerts;
import stats;
extern threshold() -> value;
extern hysteresis() -> value;
extern is_armed() -> value;
extern emit(mean, minimum, maximum, alert, next_armed) -> void;
export func evaluate() -> void {
  var avg = stats::mean();
  var lo = stats::minimum();
  var hi = stats::maximum();
  var limit = threshold();
  var band = hysteresis();
  var armed = is_armed();
  var fire = 0;
  var next = armed;
  if (avg > limit) {
    if (armed == 1) { fire = 1; }
    next = 0;
  } else {
    if (avg < limit - band) { next = 1; }
  }
  emit(avg, lo, hi, fire, next);
}
