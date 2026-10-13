// ScriptPaint pencil tool: stamps a 3x3 block at the cursor.
// Host protocol: mouse_x()/mouse_y() are value-use cursor pixels,
// pen_color() is value-use packed 0xRRGGBB, set_pixel(x, y, rgb) is
// statement-use (host ignores out-of-bounds, script still checks via
// geom::inside). Canvas is fixed 320x200 (see host kCanvasW/kCanvasH).
// paint() is void: no return expr anywhere.
module pencil;
import geom;
extern mouse_x();
extern mouse_y();
extern pen_color();
extern set_pixel(x, y, rgb);
extern present();
export func paint() {
  var mx = mouse_x();
  var my = mouse_y();
  var c = pen_color();
  var dy = -1;
  while (dy < 2) {
    var dx = -1;
    while (dx < 2) {
      var px = mx + dx;
      var py = my + dy;
      if (geom::inside(px, py, 320, 200) == 1) {
        set_pixel(px, py, c);
      }
      dx = dx + 1;
    }
    dy = dy + 1;
  }
  present();
}
