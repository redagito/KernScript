module vis_math;
import vis_util;
extern println(x) -> void;
export func f(v) -> value {
  return helper(v) + vis_util::g(v);
}
func helper(v) -> value {
  return v + 1;
}
export func id(v) -> value {
  return v + 100;
}
export func one() -> value {
  return 1;
}
export func add(a, b) -> value {
  return a + b;
}
export func show(v) -> void {
  println(v);
}
