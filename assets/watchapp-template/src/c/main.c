#include <pebble.h>

// Minimal but correct interactive watchapp skeleton.
//
// Demonstrates the conventions that keep Pebble apps stable:
//   - init() -> app_event_loop() -> deinit()
//   - a Window with .load/.unload handlers that create/destroy every layer
//   - a SELECT-button click handler wired via a ClickConfigProvider
//   - a static text buffer (text_layer_set_text stores the pointer, not a copy)
//   - layout derived from layer_get_bounds(), with a round-watch adjustment
//
// Build:   pebble build
// Run:     pebble install --emulator basalt --logs

static Window    *s_main_window;
static TextLayer *s_title_layer;
static TextLayer *s_count_layer;

static int  s_press_count = 0;
static char s_count_buffer[16];   // static: must outlive text_layer_set_text()

static void update_count_text(void) {
  snprintf(s_count_buffer, sizeof(s_count_buffer), "Pressed: %d", s_press_count);
  text_layer_set_text(s_count_layer, s_count_buffer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_press_count++;
  update_count_text();
  vibes_short_pulse();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Title
  s_title_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(40, 30), bounds.size.w, 40));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, PBL_IF_COLOR_ELSE(GColorBlue, GColorBlack));
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_text(s_title_layer, "My Watchapp");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  // Counter (press SELECT to increment)
  s_count_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(95, 85), bounds.size.w, 30));
  text_layer_set_background_color(s_count_layer, GColorClear);
  text_layer_set_text_color(s_count_layer, GColorBlack);
  text_layer_set_font(s_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_count_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_count_layer));

  update_count_text();
}

static void main_window_unload(Window *window) {
  // Destroy everything created in load (mirror order is fine for plain layers).
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_count_layer);
}

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload,
  });
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
