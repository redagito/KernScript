// ScriptPaint grayscale filter: full-canvas in-place pass, 320x200 = 64000
// get_pixel/set_pixel pairs (the stress point; host keeps the canvas size).
// Dimensions arrive via canvas_w()/canvas_h() because callFunction takes
// no arguments, so run() is nullary. run() is void. Extern consistency:
// get_pixel/canvas_w/canvas_h always value-use, set_pixel always statement.
module gray;
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
      var g = color::lum(p);
      set_pixel(x, y, color::pack(g, g, g));
      x = x + 1;
    }
    y = y + 1;
  }
  present();
}
