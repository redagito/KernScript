// Canvas line tool: integer Bresenham rasterization from the press anchor
// to the current cursor. The host restores the transaction's original pixels
// before each preview, so dragging never leaves old preview lines behind.
module lines;
extern mouse_x() -> value;
extern mouse_y() -> value;
extern stroke_x() -> value;
extern stroke_y() -> value;
extern pen_color() -> value;
extern dab_pixel(x, y, rgb) -> void;
extern present() -> void;
export func paint() -> void {
  var x = stroke_x();
  var y = stroke_y();
  var end_x = mouse_x();
  var end_y = mouse_y();
  var dx = end_x - x;
  var dy = end_y - y;
  var sx = 1;
  var sy = 1;
  if (dx < 0) { dx = 0 - dx; sx = -1; }
  if (dy < 0) { dy = 0 - dy; sy = -1; }
  dy = 0 - dy;
  var error = dx + dy;
  var done = 0;
  var color = pen_color();
  while (done == 0) {
    dab_pixel(x, y, color);
    if (x == end_x and y == end_y) {
      done = 1;
    } else {
      var twice = error * 2;
      if (twice >= dy) { error = error + dy; x = x + sx; }
      if (twice <= dx) { error = error + dx; y = y + sy; }
    }
  }
  present();
}
