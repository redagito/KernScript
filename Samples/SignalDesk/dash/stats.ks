// Window calculations stay in the embedded policy modules. The host owns
// candidate data and exposes only indexed reads for this one evaluation.
module stats;
extern sample_count() -> value;
extern sample(index) -> value;
extern window_size() -> value;
export func mean() -> value {
  var n = sample_count();
  if (n <= 0) { return 0.0; }
  var i = 0;
  var sum = 0.0;
  while (i < n) { var v = sample(i); sum = sum + v; i = i + 1; }
  return sum / n;
}
export func minimum() -> value {
  var n = sample_count();
  if (n <= 0) { return 0.0; }
  var best = sample(0);
  var i = 1;
  while (i < n) { var v = sample(i); if (v < best) { best = v; } i = i + 1; }
  return best;
}
export func maximum() -> value {
  var n = sample_count();
  if (n <= 0) { return 0.0; }
  var best = sample(0);
  var i = 1;
  while (i < n) { var v = sample(i); if (v > best) { best = v; } i = i + 1; }
  return best;
}
