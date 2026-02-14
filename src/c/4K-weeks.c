#include <pebble.h>

#include "windows/main_window.h"

#define SETTINGS_KEY 1

static Settings s_settings;

static void tick_handler(struct tm *time_now, TimeUnits changed) {
  main_window_update(time_now->tm_hour, time_now->tm_min);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read settings from the message
  Tuple *birth_year_t = dict_find(iter, MESSAGE_KEY_birthYear);
  Tuple *birth_month_t = dict_find(iter, MESSAGE_KEY_birthMonth);
  Tuple *birth_day_t = dict_find(iter, MESSAGE_KEY_birthDay);
  Tuple *gender_t = dict_find(iter, MESSAGE_KEY_gender);
  Tuple *life_expectancy_t = dict_find(iter, MESSAGE_KEY_lifeExpectancy);

  // Store settings
  if (birth_year_t) {
    s_settings.birth_year = birth_year_t->value->int32;
  }
  if (birth_month_t) {
    s_settings.birth_month = birth_month_t->value->int32;
  }
  if (birth_day_t) {
    s_settings.birth_day = birth_day_t->value->int32;
  }
  if (gender_t) {
    s_settings.gender = gender_t->value->int32;
  }
  if (life_expectancy_t) {
    s_settings.life_expectancy = life_expectancy_t->value->int32;
  }

  // Save to persistent storage
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));

  // Update the display
  main_window_refresh();
}

static void load_settings() {
  // Load settings from persistent storage or use defaults
  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  } else {
    // Default settings
    s_settings.birth_year = 1990;
    s_settings.birth_month = 1;
    s_settings.birth_day = 1;
    s_settings.gender = 0; // Male
    s_settings.life_expectancy = 80;
  }
}

Settings* get_settings() {
  return &s_settings;
}

static void init() {
  // Load saved settings
  load_settings();

  // Open AppMessage
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);

  main_window_push();

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit() {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
}

int main() {
  init();
  app_event_loop();
  deinit();
}
