# Canvas

Canvas is a PNG editor whose brushes, filters and generators are KernScript modules. The host owns image storage, undo history and the FLTK interface.

## Use and files

Build and run the `Canvas` target with GUI samples enabled. Create a 320 x 200, 640 x 400 or 960 x 600 image, or open a PNG up to 2048 x 2048. Transparency is flattened onto black. PNG is the document and export format; undo history lasts for the current session and is capped at 32 MiB of stored pixels. Use Pencil, Spray or Line to draw; filters and generators operate on the current image. Preview mode applies or cancels filter results.

## Scripts

Scripts are copied beside the executable. Edit them and select the associated tool or filter to reload; failed compilation keeps the previous VM. Brush modules export `paint() -> void`; filters export `run() -> void` and `run_band() -> void`. The host schedules filter bands between UI events and gives each VM call a finite instruction budget.

Host functions include `canvas_w()` and `canvas_h()`; `get_pixel(x,y)` and `set_pixel(x,y,rgb)`; `dab_pixel(x,y,rgb)`; pointer functions `mouse_x()`, `mouse_y()`, `mouse_down()`; `stroke_x()` and `stroke_y()`; settings `pen_color()`, `brush_radius()`, `filter_threshold()`; band bounds `band_lo()` and `band_hi()`; and `present()`. Pixels use packed `0xRRGGBB`; out-of-bounds reads return zero and writes are ignored. `filter_threshold()` is 0–255; `band_hi()` is exclusive. `dab_pixel` uses the selected opacity.

Declare readers as `-> value`; `set_pixel`, `dab_pixel` and `present` return `void`.

For example, make the pencil circular in `scripts/tools/pencil.ks` by painting only when `dx * dx + dy * dy <= radius * radius`. Shared `color` and `geom` modules affect their importers.

`Canvas --verify-headless [directory]` runs the headless verifier; files go under the supplied directory or `build/test-tmp/Canvas`. CTest registers this verifier. `Canvas --verify` opens the optional interactive verifier.
