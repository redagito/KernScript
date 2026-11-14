// Canvas grayscale filter: banded in-place pass, 320x200 = 64000
// get_pixel/set_pixel pairs (the stress point; host keeps the canvas size).
// Dimensions arrive via canvas_w()/canvas_h() and band bounds via
// band_lo()/band_hi() because callFunction takes no arguments, so
// run()/run_band() are nullary (pull model). run_band() covers
// [band_lo(), band_hi()); the host drives bands 0..H across timer ticks
// and pumps between, so the message loop stays alive. run() covers the
// whole canvas through the same rows() helper, so banded output equals
// whole-canvas output pixel-for-pixel by construction. Both are void.
// Extern consistency: get_pixel/canvas_w/canvas_h/band_lo/band_hi always
// value-use, set_pixel always statement.
module gray;
import color;
extern canvas_w() -> value;
extern canvas_h() -> value;
extern band_lo() -> value;
extern band_hi() -> value;
extern get_pixel(x, y) -> value;
extern set_pixel(x, y, rgb) -> void;
extern present() -> void;
func rows(lo, hi) -> void {
  var w = canvas_w();
  var y = lo;
  while (y < hi) {
    var x = 0;
    while (x < w) {
      var p = get_pixel(x, y);
      var g = color::lum(p);
      set_pixel(x, y, color::pack(g, g, g));
      x = x + 1;
    }
    y = y + 1;
  }
}
export func run() -> void {
  var hi = canvas_h();
  rows(0, hi);
  present();
}
export func run_band() -> void {
  var lo = band_lo();
  var hi = band_hi();
  rows(lo, hi);
  present();
}
