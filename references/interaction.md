# Interaction: Buttons, Menus & Navigation

For interactive watchapps. Buttons/clicks, MenuLayer, ActionBarLayer,
ScrollLayer, ActionMenu, SimpleMenuLayer, StatusBarLayer, NumberWindow,
multi-window navigation, vibration, and backlight. Signatures from
`developer.repebble.com/docs/c/User_Interface/...` and `/guides/events-and-services/buttons/`.

---

## Buttons / clicks

Four buttons. BACK is on the left; UP/SELECT/DOWN on the right.

```c
typedef void *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
typedef void (*ClickConfigProvider)(void *context);

typedef enum {
  BUTTON_ID_BACK, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN, NUM_BUTTONS
} ButtonId;
```

Subscribe **inside a `ClickConfigProvider`**:
```c
void window_single_click_subscribe(ButtonId id, ClickHandler handler);
void window_single_repeating_click_subscribe(ButtonId id, uint16_t repeat_interval_ms, ClickHandler handler);
void window_multi_click_subscribe(ButtonId id, uint8_t min_clicks, uint8_t max_clicks,
                                  uint16_t timeout, bool last_click_only, ClickHandler handler);
void window_long_click_subscribe(ButtonId id, uint16_t delay_ms,
                                 ClickHandler down_handler, ClickHandler up_handler);
void window_raw_click_subscribe(ButtonId id, ClickHandler down, ClickHandler up, void *context);

void window_set_click_config_provider(Window *window, ClickConfigProvider provider);
void window_set_click_config_provider_with_context(Window *window, ClickConfigProvider provider, void *context);
```

Recognizer introspection (call inside a `ClickHandler`):
```c
uint8_t  click_number_of_clicks_counted(ClickRecognizerRef recognizer);
ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer);
bool     click_recognizer_is_repeating(ClickRecognizerRef recognizer);
```

Pattern:
```c
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // single SELECT click
}
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, down_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_down, up_long_up);
}
// register after window_create():
window_set_click_config_provider(s_main_window, click_config_provider);
```
BACK is overridable but users expect it to leave the app/pop the window — don't
override it without good reason.

---

## MenuLayer

The workhorse for list UIs. `menu_layer_set_click_config_onto_window()`
auto-wires UP/DOWN scrolling and SELECT — **do not** write a ClickConfigProvider
for a MenuLayer.

```c
MenuLayer *  menu_layer_create(GRect frame);
void         menu_layer_destroy(MenuLayer *menu_layer);
Layer *      menu_layer_get_layer(const MenuLayer *menu_layer);
void         menu_layer_set_callbacks(MenuLayer *menu_layer, void *callback_context, MenuLayerCallbacks callbacks);
void         menu_layer_set_click_config_onto_window(MenuLayer *menu_layer, Window *window);
void         menu_layer_reload_data(MenuLayer *menu_layer);
void         menu_layer_set_normal_colors(MenuLayer *menu_layer, GColor bg, GColor fg);
void         menu_layer_set_highlight_colors(MenuLayer *menu_layer, GColor bg, GColor fg);
MenuIndex    menu_layer_get_selected_index(const MenuLayer *menu_layer);
void         menu_layer_set_selected_index(MenuLayer *menu_layer, MenuIndex index, MenuRowAlign align, bool animated);

// Cell-draw helpers — call from draw_row / draw_header:
void menu_cell_basic_draw(GContext *ctx, const Layer *cell, const char *title, const char *subtitle, GBitmap *icon);
void menu_cell_title_draw(GContext *ctx, const Layer *cell, const char *title);
void menu_cell_basic_header_draw(GContext *ctx, const Layer *cell, const char *title);
bool menu_cell_layer_is_highlighted(const Layer *cell);
```

`MenuIndex` is `{ uint16_t section; uint16_t row; }`. The constant
`MENU_CELL_BASIC_HEADER_HEIGHT` is the standard section-header height.

`MenuLayerCallbacks` fields (set the ones you need):
`get_num_sections`, `get_num_rows`, `get_cell_height`, `get_header_height`,
`get_separator_height`, `draw_row`, `draw_header`, `draw_separator`,
`select_click`, `select_long_click`, `selection_changed`,
`selection_will_change`, `draw_background`.

Callback signatures:
```c
uint16_t get_num_sections(MenuLayer *ml, void *data);
uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *data);
int16_t  get_header_height(MenuLayer *ml, uint16_t section, void *data);
int16_t  get_cell_height(MenuLayer *ml, MenuIndex *cell_index, void *data);
void     draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data);
void     draw_row(GContext *ctx, const Layer *cell, MenuIndex *cell_index, void *data);
void     select_click(MenuLayer *ml, MenuIndex *cell_index, void *data);
```

Minimal working menu:
```c
#define NUM_ROWS 3
static Window *s_window;
static MenuLayer *s_menu;

static uint16_t num_sections(MenuLayer *ml, void *d) { return 1; }
static uint16_t num_rows(MenuLayer *ml, uint16_t s, void *d) { return NUM_ROWS; }
static int16_t  header_h(MenuLayer *ml, uint16_t s, void *d) { return MENU_CELL_BASIC_HEADER_HEIGHT; }
static void draw_header(GContext *ctx, const Layer *cell, uint16_t s, void *d) {
  menu_cell_basic_header_draw(ctx, cell, "Items");
}
static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *d) {
  switch (idx->row) {
    case 0: menu_cell_basic_draw(ctx, cell, "First", "subtitle", NULL); break;
    case 1: menu_cell_basic_draw(ctx, cell, "Second", NULL, NULL); break;
    default: menu_cell_title_draw(ctx, cell, "Last"); break;
  }
}
static void select_click(MenuLayer *ml, MenuIndex *idx, void *d) {
  // act on idx->row — e.g. push a detail window
}
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = num_sections,
    .get_num_rows     = num_rows,
    .get_header_height= header_h,
    .draw_header      = draw_header,
    .draw_row         = draw_row,
    .select_click     = select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);   // wires buttons
  layer_add_child(root, menu_layer_get_layer(s_menu));
}
static void window_unload(Window *window) { menu_layer_destroy(s_menu); }
```

---

## ActionBarLayer

Vertical bar on the right edge with up to 3 icons (UP/SELECT/DOWN; BACK
reserved). Good for 2–3 immediate actions.

```c
ActionBarLayer *action_bar_layer_create(void);
void  action_bar_layer_destroy(ActionBarLayer *ab);
Layer *action_bar_layer_get_layer(ActionBarLayer *ab);
void  action_bar_layer_set_context(ActionBarLayer *ab, void *context);
void  action_bar_layer_set_click_config_provider(ActionBarLayer *ab, ClickConfigProvider provider);
void  action_bar_layer_set_icon(ActionBarLayer *ab, ButtonId id, const GBitmap *icon);
void  action_bar_layer_set_icon_animated(ActionBarLayer *ab, ButtonId id, const GBitmap *icon, bool animated);
void  action_bar_layer_clear_icon(ActionBarLayer *ab, ButtonId id);
void  action_bar_layer_add_to_window(ActionBarLayer *ab, Window *window);
void  action_bar_layer_remove_from_window(ActionBarLayer *ab);
void  action_bar_layer_set_background_color(ActionBarLayer *ab, GColor color);
// ACTION_BAR_WIDTH — platform-dependent width constant
```

```c
static ActionBarLayer *s_action_bar;
static GBitmap *s_icon_up, *s_icon_down;
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prev_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, next_handler);
}
// in window_load:
s_action_bar = action_bar_layer_create();
action_bar_layer_add_to_window(s_action_bar, window);
action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);
action_bar_layer_set_icon_animated(s_action_bar, BUTTON_ID_UP, s_icon_up, true);
action_bar_layer_set_icon_animated(s_action_bar, BUTTON_ID_DOWN, s_icon_down, true);
```
When laying out other content alongside the bar, reserve `ACTION_BAR_WIDTH`:
`int content_w = bounds.size.w - ACTION_BAR_WIDTH;`

---

## ScrollLayer

Scrollable viewport for content taller than the screen.
`scroll_layer_set_click_config_onto_window()` wires UP/DOWN to scroll.

```c
ScrollLayer *scroll_layer_create(GRect frame);
void  scroll_layer_destroy(ScrollLayer *sl);
Layer *scroll_layer_get_layer(const ScrollLayer *sl);
void  scroll_layer_add_child(ScrollLayer *sl, Layer *child);
void  scroll_layer_set_content_size(ScrollLayer *sl, GSize size);   // full content height
GSize scroll_layer_get_content_size(const ScrollLayer *sl);
void  scroll_layer_set_content_offset(ScrollLayer *sl, GPoint offset, bool animated);
void  scroll_layer_set_click_config_onto_window(ScrollLayer *sl, Window *window);
void  scroll_layer_set_callbacks(ScrollLayer *sl, ScrollLayerCallbacks callbacks);
void  scroll_layer_set_shadow_hidden(ScrollLayer *sl, bool hidden);
void  scroll_layer_set_paging(ScrollLayer *sl, bool paging_enabled);
// ScrollLayerCallbacks { ClickConfigProvider click_config_provider; ScrollLayerCallback content_offset_changed_handler; }
```

---

## ActionMenu (full-screen action list)

Opened from a SELECT handler; supports nested levels. Build the hierarchy once,
open on demand.

```c
ActionMenuLevel *action_menu_level_create(uint16_t max_items);
void  action_menu_level_set_display_mode(ActionMenuLevel *level, ActionMenuLevelDisplayMode mode);
ActionMenuItem *action_menu_level_add_action(ActionMenuLevel *level, const char *label,
                                             ActionMenuPerformActionCb cb, void *action_data);
ActionMenuItem *action_menu_level_add_child(ActionMenuLevel *level, ActionMenuLevel *child, const char *label);
ActionMenu *action_menu_open(ActionMenuConfig *config);
void  action_menu_close(ActionMenu *menu, bool animated);
void  action_menu_hierarchy_destroy(const ActionMenuLevel *root, ActionMenuEachItemCb each_cb, void *context);

typedef void (*ActionMenuPerformActionCb)(ActionMenu *menu, const ActionMenuItem *action, void *context);
typedef struct {
  const ActionMenuLevel *root_level;
  void *context;
  ActionMenuAlign align;
  // current SDK also exposes a .colors { background; foreground } sub-struct
} ActionMenuConfig;
// display mode: ActionMenuLevelDisplayModeWide (1/row) | ...Thin (grid)
```

---

## SimpleMenuLayer (data-driven, no draw callbacks)

Simpler than MenuLayer when you just have static titles/subtitles.

```c
SimpleMenuLayer *simple_menu_layer_create(GRect frame, Window *window,
    const SimpleMenuSection *sections, int32_t num_sections, void *callback_context);
void  simple_menu_layer_destroy(SimpleMenuLayer *ml);
Layer *simple_menu_layer_get_layer(const SimpleMenuLayer *ml);

typedef void (*SimpleMenuLayerSelectCallback)(int index, void *context);
typedef struct { const char *title; const char *subtitle; GBitmap *icon;
                 SimpleMenuLayerSelectCallback callback; } SimpleMenuItem;
typedef struct { const char *title; const SimpleMenuItem *items; uint32_t num_items; } SimpleMenuSection;
```

---

## StatusBarLayer

Thin top bar (height `STATUS_BAR_LAYER_HEIGHT`, ~16px).

```c
StatusBarLayer *status_bar_layer_create(void);
void  status_bar_layer_destroy(StatusBarLayer *sb);
Layer *status_bar_layer_get_layer(StatusBarLayer *sb);
void  status_bar_layer_set_colors(StatusBarLayer *sb, GColor background, GColor foreground);
void  status_bar_layer_set_separator_mode(StatusBarLayer *sb, StatusBarLayerSeparatorMode mode);
// mode: StatusBarLayerSeparatorModeNone | StatusBarLayerSeparatorModeDotted
```

---

## NumberWindow (built-in +/- value picker)

Signatures match the long-standing SDK (re-verify against the live symbol index
if the page path differs):
```c
NumberWindow *number_window_create(const char *label, NumberWindowCallbacks callbacks, void *context);
void   number_window_set_value(NumberWindow *nw, int32_t value);
int32_t number_window_get_value(const NumberWindow *nw);
void   number_window_set_min(NumberWindow *nw, int32_t min);
void   number_window_set_max(NumberWindow *nw, int32_t max);
void   number_window_set_step_size(NumberWindow *nw, int32_t step);
Window *number_window_get_window(NumberWindow *nw);
// NumberWindowCallbacks { NumberWindowCallback incremented; decremented; selected; }
```

---

## Navigation between windows

Push a child window from a SELECT handler; BACK pops it automatically.

```c
static Window *s_detail_window;
static void show_detail(void) {
  s_detail_window = window_create();
  window_set_window_handlers(s_detail_window, (WindowHandlers){
    .load = detail_load, .unload = detail_unload });
  window_stack_push(s_detail_window, true);
}
```
- Create the child's layers in its `.load`, destroy them in `.unload`.
- Use `.appear`/`.disappear` for state that should reset each time the window
  comes back to the top of the stack.
- Destroy the child `Window` itself (`window_destroy`) when you're done with it —
  typically track it and destroy in app `deinit`, or recreate per-push.
- Don't pop the last window; that exits the app.

---

## Vibration & backlight

```c
void vibes_short_pulse(void);
void vibes_long_pulse(void);
void vibes_double_pulse(void);
void vibes_cancel(void);
void vibes_enqueue_custom_pattern(VibePattern pattern);
// VibePattern { const uint32_t *durations; uint32_t num_segments; }  first segment = ON, ms each

void light_enable_interaction(void);   // preferred: light up briefly, auto times out
void light_enable(bool enable);        // manual on/off — drains battery if left on
```
Custom pattern:
```c
static const uint32_t segments[] = {200, 100, 400};
VibePattern pat = { .durations = segments, .num_segments = ARRAY_LENGTH(segments) };
vibes_enqueue_custom_pattern(pat);
```
