# UI & Graphics

App lifecycle, Window, Layer/drawing, TextLayer, fonts, colors, BitmapLayer, and
time. All signatures from `developer.repebble.com/docs/c/...` and the watchface
tutorial. Every `*_create` needs a matching `*_destroy` in the window `.unload`.

---

## App skeleton & lifecycle

`main()` calls `init()`, then `app_event_loop()` (blocks, routing system events
until exit), then `deinit()`.

```c
#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;

static void update_time(void) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];                 // static: outlives this call
  strftime(s_buffer, sizeof(s_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
}

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
```

---

## Window & Window Stack

```c
Window * window_create(void);
void     window_destroy(Window *window);
void     window_set_window_handlers(Window *window, WindowHandlers handlers);
Layer *  window_get_root_layer(const Window *window);
void     window_set_background_color(Window *window, GColor background_color);
void     window_set_click_config_provider(Window *window, ClickConfigProvider provider);
void     window_set_user_data(Window *window, void *data);
void *   window_get_user_data(const Window *window);
bool     window_is_loaded(Window *window);

// Window stack (navigation):
void     window_stack_push(Window *window, bool animated);
Window * window_stack_pop(bool animated);
void     window_stack_pop_all(const bool animated);
bool     window_stack_remove(Window *window, bool animated);
Window * window_stack_get_top_window(void);
bool     window_stack_contains_window(Window *window);
```

`WindowHandlers` struct — all four are `void (*)(Window *)`:
- `.load` — window pushed; **create layers here** (fires once per push).
- `.appear` — window became visible (fires every time it returns to top of
  stack). Put per-show state here (restart timers, refresh data).
- `.disappear` — window hidden (another pushed on top).
- `.unload` — window deinited; **destroy layers here**.

`.load` only fires once but `.appear` fires every return — choose accordingly.
Popping the last window kills the app; push a replacement before removing.

---

## Layer & custom drawing

A `Layer` is a positioned rectangle with an optional update procedure. Custom
drawing only happens inside a `LayerUpdateProc`, where a `GContext` is provided —
you cannot create a `GContext` yourself.

```c
Layer * layer_create(GRect frame);
Layer * layer_create_with_data(GRect frame, size_t data_size);
void    layer_destroy(Layer *layer);
void    layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc);
void    layer_mark_dirty(Layer *layer);            // request a redraw
void    layer_set_frame(Layer *layer, GRect frame);
GRect   layer_get_frame(const Layer *layer);
void    layer_set_bounds(Layer *layer, GRect bounds);
GRect   layer_get_bounds(const Layer *layer);
void    layer_add_child(Layer *parent, Layer *child);
void    layer_remove_from_parent(Layer *child);
void    layer_set_hidden(Layer *layer, bool hidden);
void *  layer_get_data(const Layer *layer);

typedef void (*LayerUpdateProc)(Layer *layer, GContext *ctx);
```

### GContext drawing primitives

```c
void graphics_context_set_stroke_color(GContext *ctx, GColor color);
void graphics_context_set_fill_color(GContext *ctx, GColor color);
void graphics_context_set_text_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width);
void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode);
void graphics_context_set_antialiased(GContext *ctx, bool enable);

void graphics_draw_pixel(GContext *ctx, GPoint point);
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1);
void graphics_draw_rect(GContext *ctx, GRect rect);
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t corner_radius, GCornerMask mask);
void graphics_draw_round_rect(GContext *ctx, GRect rect, uint16_t radius);
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius);
void graphics_draw_arc(GContext *ctx, GRect rect, GOvalScaleMode mode, int32_t start, int32_t end);
void graphics_fill_radial(GContext *ctx, GRect rect, GOvalScaleMode mode,
                          uint16_t inset_thickness, int32_t start, int32_t end);
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                        GTextOverflowMode overflow, GTextAlignment alignment,
                        const GTextAttributes *attrs);  // attrs may be NULL
```

Geometry helpers: `GRect(x,y,w,h)`, `GPoint(x,y)`, `GSize(w,h)`. Angles for
arcs/radials are in the Pebble unit where a full circle is `TRIG_MAX_ANGLE`; use
`DEG_TO_TRIGANGLE(deg)` to convert. `GCornerMask`: `GCornerNone`, `GCornersAll`,
`GCornerTopLeft`, etc.

Custom-layer pattern:
```c
static int s_battery_level;
static Layer *s_battery_layer;

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int bar_width = (s_battery_level * (bounds.size.w - 4)) / 100;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, bounds, 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(2, 2, bar_width, bounds.size.h - 4), 1, GCornerNone);
}
// in load:  s_battery_layer = layer_create(GRect(...));
//           layer_set_update_proc(s_battery_layer, battery_update_proc);
//           layer_add_child(root, s_battery_layer);
// on change: s_battery_level = ...; layer_mark_dirty(s_battery_layer);
// in unload: layer_destroy(s_battery_layer);
```

---

## TextLayer

`text_layer_set_text` stores the **pointer**, not a copy — keep the string alive.

```c
TextLayer * text_layer_create(GRect frame);
void   text_layer_destroy(TextLayer *text_layer);
void   text_layer_set_text(TextLayer *text_layer, const char *text);
void   text_layer_set_font(TextLayer *text_layer, GFont font);
void   text_layer_set_text_color(TextLayer *text_layer, GColor color);
void   text_layer_set_background_color(TextLayer *text_layer, GColor color);
void   text_layer_set_text_alignment(TextLayer *text_layer, GTextAlignment alignment);
void   text_layer_set_overflow_mode(TextLayer *text_layer, GTextOverflowMode mode);
Layer * text_layer_get_layer(TextLayer *text_layer);          // needed for layer_add_child
GSize  text_layer_get_content_size(TextLayer *text_layer);
```
`GTextAlignment`: `GTextAlignmentLeft/Center/Right`. `GTextOverflowMode`:
`GTextOverflowModeWordWrap`, `GTextOverflowModeFill`,
`GTextOverflowModeTrailingEllipsis`.

To add a TextLayer to a window you must wrap it:
`layer_add_child(root, text_layer_get_layer(s_time_layer));`

---

## Fonts

```c
GFont fonts_get_system_font(const char *font_key);     // do NOT unload these
GFont fonts_load_custom_font(ResHandle handle);
void  fonts_unload_custom_font(GFont font);
```

Common system font keys:
- GOTHIC: `FONT_KEY_GOTHIC_14`, `_14_BOLD`, `_18`, `_18_BOLD`, `_24`, `_24_BOLD`,
  `_28`, `_28_BOLD`
- BITHAM: `FONT_KEY_BITHAM_30_BLACK`, `_34_MEDIUM_NUMBERS`, `_42_BOLD`,
  `_42_LIGHT`, `_42_MEDIUM_NUMBERS`
- ROBOTO: `FONT_KEY_ROBOTO_CONDENSED_21`, `FONT_KEY_ROBOTO_BOLD_SUBSET_49`
  (49 = digits + colon only)
- DROID: `FONT_KEY_DROID_SERIF_28_BOLD`
- LECO: `FONT_KEY_LECO_20_BOLD_NUMBERS`, `_26_BOLD_NUMBERS_AM_PM`,
  `_28_LIGHT_NUMBERS`, `_32_BOLD_NUMBERS`, `_36_BOLD_NUMBERS`,
  `_38_BOLD_NUMBERS`, `_42_NUMBERS`

Custom font (declared in `resources.media` as `EXAMPLE_FONT_20`):
```c
static GFont s_font;
s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_EXAMPLE_FONT_20));
text_layer_set_font(s_time_layer, s_font);
// in unload — destroy the layer that uses it BEFORE unloading the font:
fonts_unload_custom_font(s_font);
```

---

## Colors & platform conditionals

```c
typedef GColor8 GColor;
GColorFromRGB(r, g, b);     // 0–255 each
GColorFromHEX(0x64ff46);
```
Named macros: `GColorBlack`, `GColorWhite`, `GColorClear`, `GColorRed`,
`GColorGreen`, `GColorBlue`, `GColorChromeYellow`, `GColorJaegerGreen`, etc.

Compile-time selection (see `platforms-and-memory.md` for the full set):
```c
text_layer_set_text_color(s_layer, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
GRect f = GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50);

#if defined(PBL_ROUND)
  // round-watch layout
#else
  // rectangular layout
#endif
```
Never hardcode coordinates — derive from `layer_get_bounds()` and `PBL_IF_*_ELSE`.

---

## Time & TickTimerService

```c
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler);
void tick_timer_service_unsubscribe(void);
typedef void (*TickHandler)(struct tm *tick_time, TimeUnits units_changed);
// TimeUnits: SECOND_UNIT, MINUTE_UNIT, HOUR_UNIT, DAY_UNIT, MONTH_UNIT, YEAR_UNIT
```
Use standard libc `time()` / `localtime()` / `strftime()`, plus
`clock_is_24h_style()`. Subscribe at `MINUTE_UNIT` for clocks — `SECOND_UNIT`
drains battery; only use it when seconds are genuinely shown. See the skeleton at
the top for the canonical `update_time()`.

---

## BitmapLayer & GBitmap

```c
BitmapLayer *   bitmap_layer_create(GRect frame);
void            bitmap_layer_destroy(BitmapLayer *bitmap_layer);
Layer *         bitmap_layer_get_layer(const BitmapLayer *bitmap_layer);
void            bitmap_layer_set_bitmap(BitmapLayer *bitmap_layer, const GBitmap *bitmap);
void            bitmap_layer_set_alignment(BitmapLayer *bitmap_layer, GAlign alignment);
void            bitmap_layer_set_background_color(BitmapLayer *bitmap_layer, GColor color);
void            bitmap_layer_set_compositing_mode(BitmapLayer *bitmap_layer, GCompOp mode);

GBitmap * gbitmap_create_with_resource(uint32_t resource_id);
void      gbitmap_destroy(GBitmap *bitmap);
ResHandle resource_get_handle(uint32_t resource_id);
```
`GCompOp`: `GCompOpAssign`, `GCompOpAssignInverted`, `GCompOpOr`, `GCompOpAnd`,
`GCompOpClear`, `GCompOpSet`. **Use `GCompOpSet` for transparent PNG icons.**
`GAlign`: `GAlignCenter`, `GAlignTopLeft`, `GAlignTop`, etc.

```c
static GBitmap *s_icon;
static BitmapLayer *s_icon_layer;
s_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON);
s_icon_layer = bitmap_layer_create(GRect(x, y, 30, 30));
bitmap_layer_set_bitmap(s_icon_layer, s_icon);
bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
layer_add_child(root, bitmap_layer_get_layer(s_icon_layer));
// unload — layer first, then the bitmap it references:
bitmap_layer_destroy(s_icon_layer);
gbitmap_destroy(s_icon);
```

---

## PDC vector icons (GDrawCommandImage)

**Pebble Draw Command (PDC)** files are *vector* graphics (lines, paths, circles
with baked fill/stroke colors) instead of bitmaps. Prefer them for UI glyphs
(icons, carets, hero illustrations): they're tiny (tens to a few hundred bytes),
crisp, and recolorable. They are **not** a `BitmapLayer` — you draw a
`GDrawCommandImage` yourself inside a `LayerUpdateProc`.

Declare the `.pdc` in `package.json` as a **`raw`** resource (not `bitmap`):
```json
{ "type": "raw", "name": "QUIET_TIME", "file": "quiet_time.pdc" }
```

```c
GDrawCommandImage * gdraw_command_image_create_with_resource(uint32_t resource_id);
void                gdraw_command_image_destroy(GDrawCommandImage *image);
void                gdraw_command_image_draw(GContext *ctx, GDrawCommandImage *image, GPoint offset);
GSize               gdraw_command_image_get_bounds_size(GDrawCommandImage *image);
GDrawCommandImage * gdraw_command_image_clone(GDrawCommandImage *image);
```

Pattern — a plain `Layer` whose update proc draws the image at its origin. Size
the layer from the image's own bounds; the glyph's colors come from the file:
```c
static GDrawCommandImage *s_icon;
static Layer *s_icon_layer;

static void icon_update(Layer *layer, GContext *ctx) {
  if (s_icon) gdraw_command_image_draw(ctx, s_icon, GPointZero);
}
// load:
s_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_QUIET_TIME);
GSize sz = gdraw_command_image_get_bounds_size(s_icon);
s_icon_layer = layer_create(GRect(x, y, sz.w, sz.h));
layer_set_update_proc(s_icon_layer, icon_update);
layer_add_child(root, s_icon_layer);
// unload — layer first, then the image:
layer_destroy(s_icon_layer);
gdraw_command_image_destroy(s_icon);
```

Gotchas (each one bites in practice):
- **Colors are baked in.** Author the SVG in the *final* on-watch colors — a
  `stroke="black"` glyph is invisible on a black background. There is no
  `GCompOpSet`-style tinting; what's in the file is what draws.
- **No draw-time scaling.** `gdraw_command_image_draw` renders at the authored
  coordinates. To change size, regenerate the PDC at the target size — **do not**
  downscale a large PDC into a small layer; thin strokes get smeared by
  anti-aliasing. (`gdraw_command_image_set_bounds_size` sets the *reported*
  bounds, it does not rescale the drawing.) Pick the size before converting.
- **No curves.** PDC stores only line segments and circles; cubic/quadratic
  béziers in the SVG are approximated to polylines at convert time. Coordinates
  snap to a half-pixel grid (the converter warns when it rounds).
- **Menu icons must stay bitmaps** — a `menuIcon` resource can't be PDC. Keep
  `icon.png` a `bitmap`.

**Authoring/tooling (SVG → PDC).** Two converters:
- [`pdc_tool`](https://github.com/HBehrens/pdc_tool) (Heiko Behrens) — a single
  self-contained binary, maintained. Subcommands: `pdc` (write PDC), `png`
  (render a PDC or SVG to PNG — invaluable for previewing color/size/legibility
  before building), `info`. e.g. `pdc_tool icon.svg pdc icon.pdc` and
  `pdc_tool icon.pdc png preview.png --background-color indigo --crop 0`.
- [`svg2pdc.py`](https://github.com/pebble-examples/cards-example/blob/master/tools/svg2pdc.py)
  — the classic pure-Python script from Pebble examples; commits cleanly (no
  binary) but is Python-2-era (needs a 2→3 port + `svg.path`) and has no preview.

Both emit identical PDC, so the C code and manifest don't depend on which you
use. Keep the `.svg` masters in `resources/` next to the generated `.pdc`, and a
small regen script so the committed PDC always matches its source.

**Animated PDC (`GDrawCommandSequence`).** Multi-frame vector animations (also a
`raw` resource) play through `gdraw_command_sequence_*`: create with resource,
`gdraw_command_sequence_get_frame_by_elapsed(seq, elapsed_ms)` to pick the frame,
then `gdraw_command_frame_draw(ctx, seq, frame, offset)` in the update proc;
drive `elapsed` from an `AppTimer` and `layer_mark_dirty`.
