// ScriptPaint color library: packed RGB helpers shared by filters/tools.
//
// Packed format is 0xRRGGBB as a script int. 0xFFFFFF = 16777215 < 2^31,
// so every packed color stays a signed-safe positive int32; no unsigned
// literals and no overflow anywhere below (r*65536 peaks at 16711680).
// String law: no strings here at all. All funcs are valued (return expr).
module color;
export func pack(r, g, b) {
  return r * 65536 + g * 256 + b;
}
export func r(p) {
  return p / 65536;
}
export func g(p) {
  return p / 256 - r(p) * 256;
}
export func b(p) {
  return p - r(p) * 65536 - g(p) * 256;
}
export func lum(p) {
  return (r(p) + g(p) + b(p)) / 3;
}
export func mix(a, b) {
  return pack((r(a) + r(b)) / 2, (g(a) + g(b)) / 2, (b(a) + b(b)) / 2);
}
