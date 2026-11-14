module c1;
import c2;
export func f1() -> value {
  return c2::f2() + 1;
}
