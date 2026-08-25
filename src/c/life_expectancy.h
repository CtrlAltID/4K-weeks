#pragma once

/**
 * Life expectancy at birth by country, for the "weeks left" pill.
 *
 * Figures are rough averages assembled from widely-published UN / World Bank
 * estimates (circa early 2020s). They are meant to give a personally
 * meaningful ballpark, not actuarial or medical advice — actual life
 * expectancy varies enormously by individual health, income, and the year
 * you're reading this. `country_code` is an ISO 3166-1 alpha-2 code (e.g.
 * "US", "GB", "JP"), matching what a phone's reverse-geocoding lookup
 * returns, so the same table serves both the manual Country picker and the
 * location-based estimate. "WORLD" is the fallback used when a code isn't in
 * the table.
 *
 * The list favors coverage of easily-sourced, populous, or well-documented
 * countries over completeness — nothing near all ~195 are here.
 */

#include <string.h>
#include <ctype.h>

typedef struct {
  const char *code;
  float male;
  float female;
} CountryLifeExpectancy;

static const CountryLifeExpectancy s_life_table[] = {
  {"US", 76.3f, 81.4f},  // United States
  {"GB", 79.0f, 82.9f},  // United Kingdom
  {"CA", 80.4f, 84.4f},  // Canada
  {"AU", 81.2f, 85.3f},  // Australia
  {"NZ", 80.5f, 83.9f},  // New Zealand
  {"IE", 80.4f, 84.0f},  // Ireland
  {"DE", 78.7f, 83.4f},  // Germany
  {"FR", 79.7f, 85.6f},  // France
  {"IT", 80.8f, 85.2f},  // Italy
  {"ES", 80.9f, 86.2f},  // Spain
  {"PT", 78.6f, 84.5f},  // Portugal
  {"NL", 80.2f, 83.4f},  // Netherlands
  {"BE", 79.4f, 84.0f},  // Belgium
  {"CH", 81.9f, 85.6f},  // Switzerland
  {"AT", 79.3f, 84.0f},  // Austria
  {"SE", 81.3f, 84.7f},  // Sweden
  {"NO", 81.6f, 84.7f},  // Norway
  {"DK", 79.8f, 83.6f},  // Denmark
  {"FI", 79.2f, 84.5f},  // Finland
  {"IS", 80.9f, 84.1f},  // Iceland
  {"PL", 74.7f, 82.1f},  // Poland
  {"CZ", 76.1f, 82.0f},  // Czechia
  {"SK", 74.4f, 81.1f},  // Slovakia
  {"HU", 73.0f, 80.1f},  // Hungary
  {"RO", 71.9f, 79.2f},  // Romania
  {"BG", 71.5f, 78.5f},  // Bulgaria
  {"GR", 78.8f, 84.0f},  // Greece
  {"HR", 75.5f, 81.6f},  // Croatia
  {"SI", 78.3f, 84.1f},  // Slovenia
  {"RU", 66.5f, 76.9f},  // Russia
  {"UA", 65.5f, 76.0f},  // Ukraine
  {"TR", 75.9f, 81.3f},  // Turkey
  {"IL", 80.6f, 84.4f},  // Israel
  {"SA", 74.8f, 78.6f},  // Saudi Arabia
  {"AE", 76.4f, 80.4f},  // United Arab Emirates
  {"CN", 74.7f, 80.5f},  // China
  {"JP", 81.5f, 87.6f},  // Japan
  {"KR", 80.6f, 86.6f},  // South Korea
  {"IN", 68.6f, 71.4f},  // India
  {"PK", 65.6f, 68.1f},  // Pakistan
  {"BD", 71.0f, 74.5f},  // Bangladesh
  {"ID", 69.9f, 73.9f},  // Indonesia
  {"TH", 73.5f, 80.0f},  // Thailand
  {"VN", 71.0f, 80.2f},  // Vietnam
  {"PH", 67.4f, 73.0f},  // Philippines
  {"MY", 73.5f, 78.0f},  // Malaysia
  {"SG", 81.0f, 86.1f},  // Singapore
  {"BR", 73.1f, 79.9f},  // Brazil
  {"MX", 72.6f, 78.1f},  // Mexico
  {"AR", 73.5f, 80.3f},  // Argentina
  {"CL", 77.0f, 82.5f},  // Chile
  {"CO", 73.0f, 80.5f},  // Colombia
  {"PE", 73.0f, 78.5f},  // Peru
  {"ZA", 62.4f, 68.6f},  // South Africa
  {"NG", 53.0f, 55.9f},  // Nigeria
  {"EG", 69.9f, 74.5f},  // Egypt
  {"KE", 62.0f, 67.6f},  // Kenya
  {"ET", 65.0f, 68.5f},  // Ethiopia
  {"GH", 63.0f, 65.7f},  // Ghana
  {"WORLD", 70.8f, 75.9f},  // Global average, used as the fallback
};
#define LIFE_TABLE_LEN (sizeof(s_life_table) / sizeof(s_life_table[0]))

/**
 * Years of life expectancy for a country code and gender. `country_code`
 * must already be upper-cased (the caller normalizes it on ingestion).
 * gender: 0 = male, 1 = female, 2 = other/unspecified -> average of the two.
 * An unrecognized or empty code falls back to the "WORLD" entry.
 */
static inline float life_expectancy_years(const char *country_code, uint8_t gender) {
  const CountryLifeExpectancy *hit = NULL;
  if (country_code && country_code[0]) {
    for (unsigned i = 0; i < LIFE_TABLE_LEN; i++) {
      if (strcmp(country_code, s_life_table[i].code) == 0) {
        hit = &s_life_table[i];
        break;
      }
    }
  }
  if (!hit) {
    hit = &s_life_table[LIFE_TABLE_LEN - 1];  // "WORLD"
  }
  if (gender == 0) {
    return hit->male;
  }
  if (gender == 1) {
    return hit->female;
  }
  return (hit->male + hit->female) / 2.0f;
}
