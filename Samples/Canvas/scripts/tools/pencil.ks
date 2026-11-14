// Canvas pencil tool: an adjustable square brush with host-blended opacity.
// Host protocol: mouse_x()/mouse_y() are value-use cursor pixels,
// pen_color() is value-use packed 0xRRGGBB, set_pixel(x, y, rgb) is
// statement-use (host ignores out-of-bounds, script still checks via
// geom::inside). Dimensions and brush radius come from the current document.
// paint() is void: no return expr anywhere.
module pencil;
import geom;
extern mouse_x() -> value;
extern mouse_y() -> value;
extern pen_color() -> value;
extern dab_pixel(x, y, rgb) -> void;
extern brush_radius() -> value;
extern canvas_w() -> value;
extern canvas_h() -> value;
extern present() -> void;
export func paint() -> void {
  var mx = mouse_x();
  var my = mouse_y();
  var c = pen_color();
  var radius = brush_radius();
  var w = canvas_w();
  var h = canvas_h();
  var dy = 0 - radius;
  while (dy <= radius) {
    var dx = 0 - radius;
    while (dx <= radius) {
      var px = mx + dx;
      var py = my + dy;
      if (geom::inside(px, py, w, h) == 1) {
        dab_pixel(px, py, c);
      }
      dx = dx + 1;
    }
    dy = dy + 1;
  }
  present();
}
