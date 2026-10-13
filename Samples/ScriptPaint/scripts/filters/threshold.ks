// ScriptPaint threshold filter: lum > 128 goes white else black, in place.
// Third importer of color (gray and edge are the others), so any root that
// pulls two filters forms a diamond through color. run() is void.
module threshold;
import color;
extern canvas_w();
extern canvas_h();
extern get_pixel(x, y);
extern set_pixel(x, y, rgb);
extern present();
export func run() {
  var w = canvas_w();
  var h = canvas_h();
  var y = 0;
  while (y < h) {
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
  present();
}
