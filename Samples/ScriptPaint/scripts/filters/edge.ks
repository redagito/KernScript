// ScriptPaint edge filter: cross-neighbor luminance gradient, in place.
// Banded form: run_band() covers [band_lo(), band_hi()); the host drives
// bands 0..H across timer ticks. Five get_pixel calls per pixel (320000
// extern reads over the canvas). Borders clamp by hand against the FULL
// canvas extents (this module imports color only, no geom), so band splits
// keep the exact whole-canvas order and output. run() covers the whole
// canvas through the same rows() helper, so banded output equals
// whole-canvas output pixel-for-pixel by construction. Both are void.
// Output is 0 or 0xFFFFFF (16777215, signed-safe).
module edge;
import color;
extern canvas_w();
extern canvas_h();
extern band_lo();
extern band_hi();
extern get_pixel(x, y);
extern set_pixel(x, y, rgb);
extern present();
func rows(lo, hi) {
  var w = canvas_w();
  var h = canvas_h();
  var y = lo;
  while (y < hi) {
    var x = 0;
    while (x < w) {
      var x0 = x - 1;
      if (x0 < 0) {
        x0 = 0;
      }
      var x1 = x + 1;
      if (x1 > w - 1) {
        x1 = w - 1;
      }
      var y0 = y - 1;
      if (y0 < 0) {
        y0 = 0;
      }
      var y1 = y + 1;
      if (y1 > h - 1) {
        y1 = h - 1;
      }
      var pl = get_pixel(x0, y);
      var pr = get_pixel(x1, y);
      var pu = get_pixel(x, y0);
      var pd = get_pixel(x, y1);
      var gx = color::lum(pr) - color::lum(pl);
      var gy = color::lum(pd) - color::lum(pu);
      var m = gx + gy;
      if (m < 0) {
        m = 0 - m;
      }
      var out = 0;
      if (m > 32) {
        out = 16777215;
      }
      set_pixel(x, y, out);
      x = x + 1;
    }
    y = y + 1;
  }
}
export func run() {
  var hi = canvas_h();
  rows(0, hi);
  present();
}
export func run_band() {
  var lo = band_lo();
  var hi = band_hi();
  rows(lo, hi);
  present();
}
