// Canvas spray tool: deterministic splatter around the cursor.
// No host random extern exists, so the pattern hashes the cursor plus the
// drop index (all non-negative, so int division truncation never matters).
// Modulo is longhand (v - (v / d) * d): v1 has no % operator.
module spray;
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
  var height = canvas_h();
  var i = 0;
  while (i < 24) {
    var h = mx * 73 + my * 151 + i * 37 + 11;
    var ox = h - (h / 7) * 7 - 3;
    var h2 = h / 7 + i * 13;
    var oy = h2 - (h2 / 7) * 7 - 3;
    var px = mx + ox * radius;
    var py = my + oy * radius;
    if (geom::inside(px, py, w, height) == 1) {
      dab_pixel(px, py, c);
    }
    i = i + 1;
  }
  present();
}
