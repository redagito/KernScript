// ScriptPaint threshold filter: lum > 128 goes white else black, in place.
// Banded form: run_band() covers [band_lo(), band_hi()); the host drives
// bands 0..H across timer ticks. Third importer of color (gray and edge
// are the others), so any root that pulls two filters forms a diamond
// through color. run() covers the whole canvas through the same rows()
// helper, so banded output equals whole-canvas output pixel-for-pixel by
// construction. Both are void.
module threshold;
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
  var y = lo;
  while (y < hi) {
    var x = 0;
    while (x < w) {
      var p = get_pixel(x, y);
      var l = color::lum(p);
      var out = 0;
      if (l > 128) {
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
