// ScriptPaint spray tool: deterministic splatter around the cursor.
// No host random extern exists, so the pattern hashes the cursor plus the
// drop index (all non-negative, so int division truncation never matters).
// Modulo is longhand (v - (v / d) * d): v1 has no % operator.
module spray;
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
  var i = 0;
  while (i < 24) {
    var h = mx * 73 + my * 151 + i * 37 + 11;
    var ox = h - (h / 7) * 7 - 3;
    var h2 = h / 7 + i * 13;
    var oy = h2 - (h2 / 7) * 7 - 3;
    var px = mx + ox;
    var py = my + oy;
    if (geom::inside(px, py, 320, 200) == 1) {
      set_pixel(px, py, c);
    }
    i = i + 1;
  }
  present();
}
