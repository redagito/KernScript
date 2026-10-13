module vis_math;
import vis_util;
extern println(x);
export func f(v) {
  return helper(v) + vis_util::g(v);
}
func helper(v) {
  return v + 1;
}
export func id(v) {
  return v + 100;
}
export func one() {
  return 1;
}
export func add(a, b) {
  return a + b;
}
export func show(v) {
  println(v);
}
