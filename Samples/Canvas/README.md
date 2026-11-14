# Canvas

Canvas is a small PNG image editor whose brushes, filters, and procedural
generators run as KernScript modules. The host provides a document, image
storage, undo history, and a portable FLTK interface.

## Make and save an image

1. Select a document size and click **New**, or **Open** an existing PNG.
2. Choose Pencil, Spray, or Line. Drag on the canvas to paint. Adjust brush
   radius and opacity; choose a swatch or open **Choose color**.
3. Try Grayscale, Edges, Threshold, or Mandelbrot. With **Preview filters**
   enabled, **Apply** keeps the result and **Cancel** restores the original.
   Adjusting Threshold after its preview finishes recalculates the preview
   from the original image.
4. Use **Undo** and **Redo** for strokes, clears, and applied filters.
5. **Save** writes a PNG; **Save As** chooses another destination. Unsaved
   changes are indicated in the title and checked before closing or opening
   another document.

New documents are 320 x 200, 640 x 400, or 960 x 600. PNGs up to 2048 x 2048
can be opened. The viewport fits images while preserving their aspect ratio.
Transparency is flattened onto black when opening a PNG. PNG is the editable
document and export format; undo history is kept only for the current session.
History is limited to 32 MiB of stored pixels, dropping oldest edits as needed.

## Customize a tool

Scripts are copied beside the executable. Edit those files, then click the
tool or filter again to reload it. A failed compilation keeps the previous
working VM. Compiler diagnostics appear in the error dialog and on stderr.
No C++ rebuild is needed to change the brush or filter algorithm.

Try changing the square pencil into a circular brush: in
`scripts/tools/pencil.ks`, paint only when
`dx * dx + dy * dy <= radius * radius`. Or modify
`scripts/filters/threshold.ks` to output two colors of your choice.

| Host function | Meaning |
| --- | --- |
| `canvas_w()`, `canvas_h()` | Current document dimensions |
| `get_pixel(x, y)` | Read packed `0xRRGGBB`; out-of-bounds reads return zero |
| `set_pixel(x, y, rgb)` | Replace a pixel; out-of-bounds writes are ignored |
| `dab_pixel(x, y, rgb)` | Paint a pixel using the selected opacity |
| `mouse_x()`, `mouse_y()`, `mouse_down()` | Document coordinates and button state |
| `stroke_x()`, `stroke_y()` | Initial press coordinates for an anchored line |
| `pen_color()`, `brush_radius()` | Current brush settings |
| `filter_threshold()` | Threshold slider value, from 0 to 255 |
| `band_lo()`, `band_hi()` | Half-open row range for the current filter batch |
| `present()` | Refresh the displayed image |

Brush modules export `paint() -> void`. Filters export `run() -> void` and
`run_band() -> void`. The host schedules filter bands between UI events;
the VM itself runs each call to completion with a finite instruction budget.
Editing the shared `color` and `geom` modules also changes their importers.

The Line tool previews a straight line from the initial press position to the
cursor. Releasing the mouse commits it as one undoable edit. Brush radius
controls Pencil and Spray; Line draws a one-pixel path.

## Verification and larger projects

`Canvas --verify-headless [directory]` exercises real script execution, larger
document coordinates, brush settings, PNG round trips, undo, cancelled
previews, and failed reload preservation without creating a window. CTest
registers this verifier. Files are written to the supplied directory, or
`build/test-tmp/Canvas` when none is supplied. Unit tests cover document
transactions, saved state, history branching, and memory limits.

`Canvas --verify` is the optional interactive verifier and opens a window.

For a realistic larger workload, open a 960 x 600 image, apply several filters,
undo and redo them, and generate a Mandelbrot background. Larger documents
exercise script loops and host calls while band scheduling keeps work in
separate calls. Preview cancellation discards partial processing.
