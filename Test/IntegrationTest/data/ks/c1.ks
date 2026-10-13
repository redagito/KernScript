module c1;
import c2;
export func f1() {
  return c2::f2() + 1;
}
