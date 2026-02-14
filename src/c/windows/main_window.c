#include "main_window.h"

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_weeks_layer;

static int s_hours, s_minutes;
static char s_weeks_buffer[16];

static int32_t get_angle_for_hour(int hour) {
  // Progress through 12 hours, out of 360 degrees
  return (hour * 360) / 12;
}

static int32_t get_angle_for_minute(int minute) {
  // Progress through 60 minutes, out of 360 degrees
  return (minute * 360) / 60;
}

static int calculate_weeks_remaining() {
  // Get current time
  time_t now = time(NULL);

  // Get settings
  Settings *settings = get_settings();

  // Build birthdate from settings
  struct tm birth_date = {
    .tm_year = settings->birth_year - 1900,  // Years since 1900
    .tm_mon = settings->birth_month - 1,      // Months are 0-11
    .tm_mday = settings->birth_day,
    .tm_hour = 0,
    .tm_min = 0,
    .tm_sec = 0
  };

  time_t birth_timestamp = mktime(&birth_date);

  // Calculate weeks lived
  int seconds_per_week = 60 * 60 * 24 * 7;
  int weeks_lived = (now - birth_timestamp) / seconds_per_week;

  // Use life expectancy from settings
  int total_weeks = settings->life_expectancy * 52;
  int weeks_remaining = total_weeks - weeks_lived;

  // Don't show negative numbers
  if (weeks_remaining < 0) {
    weeks_remaining = 0;
  }

  return weeks_remaining;
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // 12 hours only, with a minimum size
  s_hours -= (s_hours > 12) ? 12 : 0;

  // Minutes are expanding circle arc
  int minute_angle = get_angle_for_minute(s_minutes);
  GRect frame = grect_inset(bounds, GEdgeInsets(4 * INSET));
  graphics_context_set_fill_color(ctx, MINUTES_COLOR);
  graphics_fill_radial(ctx, frame, GOvalScaleModeFitCircle, 20, 0, DEG_TO_TRIGANGLE(minute_angle));

  // Adjust geometry variables for inner ring
  frame = grect_inset(frame, GEdgeInsets(3 * HOURS_RADIUS));

  // Hours are dots
  for(int i = 0; i < 12; i++) {
    int hour_angle = get_angle_for_hour(i);
    GPoint pos = gpoint_from_polar(frame, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(hour_angle));

    graphics_context_set_fill_color(ctx, i <= s_hours ? HOURS_COLOR : HOURS_COLOR_INACTIVE);
    graphics_fill_circle(ctx, pos, HOURS_RADIUS);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, layer_update_proc);
  layer_add_child(window_layer, s_canvas);

  // Create text layer for weeks remaining
  int weeks = calculate_weeks_remaining();
  snprintf(s_weeks_buffer, sizeof(s_weeks_buffer), "%d", weeks);

  s_weeks_layer = text_layer_create(GRect(0, bounds.size.h / 2 - 20, bounds.size.w, 40));
  text_layer_set_text(s_weeks_layer, s_weeks_buffer);
  text_layer_set_text_alignment(s_weeks_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_weeks_layer, GColorClear);
  text_layer_set_text_color(s_weeks_layer, GColorWhite);
  text_layer_set_font(s_weeks_layer, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS));
  layer_add_child(window_layer, text_layer_get_layer(s_weeks_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_weeks_layer);
  layer_destroy(s_canvas);
  window_destroy(s_window);
}

void main_window_push() {
  s_window = window_create();
  window_set_background_color(s_window, BG_COLOR);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void main_window_update(int hours, int minutes) {
  s_hours = hours;
  s_minutes = minutes;
  layer_mark_dirty(s_canvas);
}

void main_window_refresh() {
  // Recalculate and update weeks display
  int weeks = calculate_weeks_remaining();
  snprintf(s_weeks_buffer, sizeof(s_weeks_buffer), "%d", weeks);
  text_layer_set_text(s_weeks_layer, s_weeks_buffer);
}
