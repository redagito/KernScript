// Aggregates execute inside KernScript. The host owns ranges and dependencies.
module sheet;
extern range_count(id) -> value;
extern range_value(id, index) -> value;
extern range_numeric(id, index) -> value;

export func sum(id) -> value {
  var total = 0.0;
  var i = 0;
  while (i < range_count(id)) {
    if (range_numeric(id, i)) { total = total + range_value(id, i); }
    i = i + 1;
  }
  return total;
}
export func count(id) -> value {
  var count = 0;
  var i = 0;
  while (i < range_count(id)) {
    if (range_numeric(id, i)) { count = count + 1; }
    i = i + 1;
  }
  return count;
}
export func average(id) -> value {
  var n = count(id);
  if (n == 0) { return 0.0; }
  return sum(id) / n;
}
export func minimum(id) -> value {
  var first = 1;
  var result = 0.0;
  var i = 0;
  while (i < range_count(id)) {
    if (range_numeric(id, i)) {
      var value = range_value(id, i);
      if (first or value < result) { result = value; first = 0; }
    }
    i = i + 1;
  }
  return result;
}
export func maximum(id) -> value {
  var first = 1;
  var result = 0.0;
  var i = 0;
  while (i < range_count(id)) {
    if (range_numeric(id, i)) {
      var value = range_value(id, i);
      if (first or value > result) { result = value; first = 0; }
    }
    i = i + 1;
  }
  return result;
}
// An ordinary numeric helper can be called as sheet::percent(A1, 12.5).
export func percent(amount, rate) -> value { return amount * rate / 100.0; }
