// ScriptPaint line tool: stamps a 21-pixel diagonal segment through the
// cursor each tick. Anchor-free by design: the fixed host extern set has
// no line-anchor call, so a press-drag rubber band cannot reach the script;
// the timer tick re-stamps while held, which still draws a line as the
// cursor moves. paint() is void.
module lines;
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
  var i = -10;
  while (i < 11) {
    var px = mx + i;
    var py = my + i;
    if (geom::inside(px, py, 320, 200) == 1) {
      set_pixel(px, py, c);
    }
    i = i + 1;
  }
  present();
}
