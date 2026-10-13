// ScriptPaint Mandelbrot generator: fixed-point integer escape-time.
//
// Script ints are 32-bit and floats are 32-bit, so this stays in integers:
// coordinates are scaled by 256 (cr covers -640..253 = -2.5..1.0 real,
// ci covers -256..253 = -1.0..1.0 imag). Overflow audit: x*896 peaks at
// 285824; inside the loop zr peaks near 4349, so zr*zr peaks near 19M and
// mag = zr*zr + zi*zi peaks near 38M, all far below 2^31. Escape radius
// squared is 4*256*256 = 1048576. Iteration cap is 30 (spec: low cap).
// No break/continue in v1: escape encodes iter as 30 + count, then the
// count is decoded after the loop. Shade is 8 + count*8 (8..248), so every
// pixel lands nonzero and --verify can prove full-canvas coverage.
// run() is void.
module mandel;
import color;
extern canvas_w();
extern canvas_h();
extern set_pixel(x, y, rgb);
extern present();
export func run() {
  var w = canvas_w();
  var h = canvas_h();
  var y = 0;
  while (y < h) {
    var x = 0;
    while (x < w) {
      var cr = x * 896 / 320 - 640;
      var ci = y * 512 / 200 - 256;
      var zr = 0;
      var zi = 0;
      var iter = 0;
      while (iter < 30) {
        var mag = zr * zr + zi * zi;
        if (mag > 1048576) {
          iter = 30 + iter;
        } else {
          var nzr = (zr * zr - zi * zi) / 256 + cr;
          var nzi = (2 * zr * zi) / 256 + ci;
          zr = nzr;
          zi = nzi;
          iter = iter + 1;
        }
      }
      var count = iter;
      if (count > 30) {
        count = count - 30;
      }
      var shade = 8 + count * 8;
      set_pixel(x, y, color::pack(shade, shade, shade));
      x = x + 1;
    }
    y = y + 1;
  }
  present();
}
