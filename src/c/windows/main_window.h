#pragma once

#include <pebble.h>

#include "../config.h"

typedef struct {
  int birth_year;
  int birth_month;
  int birth_day;
  int gender;
  int life_expectancy;
} Settings;

void main_window_push();

void main_window_update(int hours, int minutes);

void main_window_refresh();

Settings* get_settings();
