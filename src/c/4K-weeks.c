/**
 * 4K-weeks — analog watchface for Pebble Time 2 (emery) and Round 2 (gabbro).
 *
 * A one-hand dial: the hand stands still and the hour scale rolls past it. The
 * scale sits on a circle whose center is C_RADIUS behind the screen center,
 * opposite the hand, so the visible arc is nearly straight. Design constants
 * are in reference units against a REF_H-tall screen and scaled at runtime, so
 * one set of numbers serves both displays.
 *
 * A pill grows in from its own center on a wrist shake: first the date, a
 * second shake the battery gauge, a third the number of weeks left to live
 * given a configured birthday, gender, and country's life expectancy.
 *
 * Sections: design constants, settings and state, geometry, type, dial, date
 * pill, pill animation and state, services, and setup at the end.
 *
 * Adapted from Line Dash (github.com/pagnotta/pebble-line-dash) by
 * Patrick Heeren (pagnotta), MIT licensed — see THIRD-PARTY-NOTICES.md. The
 * dial and bitmap numeral rendering below are almost entirely his; the
 * weeks-left page, its settings, and the pill's grow/shrink animation are
 * new (the original slid the pill on and off the top of the screen).
 *
 * @license MIT
 */

#include <pebble.h>
#include <ctype.h>
#include <stdlib.h>
#include <locale.h>
#include "message_keys.auto.h"
#include "life_expectancy.h"

// ==========================================================================
// Design constants, in reference units
// ==========================================================================

// s_scale maps this height onto the short display edge. gabbro uses a larger
// reference so that its 260 px render at emery's scale rather than 30 % larger:
// stroke weights match on both, and the bigger display shows more of the dial
// instead of a magnified crop of it.
#if PBL_DISPLAY_WIDTH == 260
#define REF_H 229.0f
#else
#define REF_H 176.0f
#endif
// --- Dial ---
#define C_RADIUS 180.0f     // offset of the scale rotation center
#define C_HOUR_LEN 24.0f    // full-hour mark, all three scales
#define C_HOUR_SLEN 17.0f   // half-hour mark, all three scales
#define C_HOUR_W 6.5f       // tick width
#define C_HAND_W 6.0f       // needle hand width
#define C_HAND_FULL_W 8.0f  // full-length hand width (reads thinner on e-paper, so heavier)
#define C_NUM_SIZE 22.0f    // minute circle radius

// The ruled scales draw their subdivisions as thin bars. All three scales
// share the hour and half-hour lengths above and differ only in what sits
// between them. Length alone cannot rank the hour and half hour here, so on
// these scales both also take the needle's weight and square ends.
#define C_FINE_W 3.0f          // subdivision bar width
#define C_FINE_HOUR_W 6.0f     // hour and half hour, at the needle weight
#define C_FINE_TEN_LEN 10.0f   // 10-minute bar, tenths scale
#define C_QUARTER_LEN 13.0f    // quarter past and to, quarters scale
#define C_FIVE_LEN 6.0f        // 5-minute bar, quarters scale

// The tenths scale marks five minutes with a dot rather than a bar: a fifth
// bar length would have to be a hairline, finer than the display resolves
// cleanly. A dot stays small while keeping the full stroke weight.
#define C_DOT5_W 3.0f          // 5-minute dot, tenths scale only

// --- Date pill ---
// Grows in from its own center, in place, rather than sliding on screen.
// This is the only thing that ever puts the pill away during normal use —
// there's no tap gesture that means "close," so 0 would not mean "never
// times out": app_timer_register(0, ...) fires on the next tick, closer to
// instant.
//
// Sized off the backlight rather than left open-ended: the pill has no
// reason to keep the screen effectively "on" (readable, meant to be looked
// at) well past the point the backlight itself has already timed out and
// gone dark. There's no SDK API to read the user's actual configured
// backlight duration, so this targets roughly double the platform's max
// (8s), which stays comfortably past a real backlight cycle without
// drifting toward "stays up indefinitely."
//
// Unrelated to SHAKE_BURST_* below: that's what actually guards against the
// pill popping up from sustained tap noise during a run or ride. This
// timeout only governs how long an already-open pill lingers before
// auto-closing back to the plain dial.
#define PILL_TIMEOUT_MS 16000
#define PILL_REVEAL_MS 300
#define PILL_FILL_MS 450      // battery level sweeping in / out
#define C_PILL_HALF_H 29.0f   // pill half height
#define C_PILL_PAD 14.0f      // horizontal padding beyond the widest text line
#define C_PILL_BORDER 4.0f    // border thickness
#define C_PILL_R 10.0f        // corner radius
#define C_PILL_GAP 4.0f       // dark gap between the border and the battery level
#define C_BOLT_W 9.0f         // charging bolt, drawn inside the pill on the left
#define C_BOLT_H 22.0f
#define C_BOLT_GAP 6.0f       // space between the bolt and the percentage

// Cycling and running keep the tap sensor firing. SHAKE_BURST_N taps spaced
// less than SHAKE_BURST_S apart count as sustained motion and the pill leaves;
// the count sits above the taps it takes to page all the way through it. With
// the weeks-left page added there are three stops rather than two, so a full
// walk-around-and-back (open, then advance past battery and life to date
// again) is four taps on its own — SHAKE_BURST_N needs headroom above that,
// not level with it.
// Coming back originally needed the longer calm of a full minute, so a pause
// at a junction mid-ride would not hand the pill back for the next stretch of
// road. Deliberately traded away here for a snappier SHAKE_RELEASE_S: a lull
// of just a few seconds — a red light, a downhill coast — now re-arms it, so
// this is closer to no protection during sustained exercise than real
// protection. Fine for normal wear where the goal is not fighting the mute
// during testing; raise it back toward 60 if the pill starts popping up
// mid-run or mid-ride.
//
// MOTION_MUTE_ENABLED off lets every tap register regardless of how often
// they come, which is friendlier while poking at the pill repeatedly to test
// it — flip to 0 again for that, but leave it at 1 for normal wear, or the
// pill pops up on every stride of a run or ride.
#define MOTION_MUTE_ENABLED 1
#define SHAKE_BURST_S 20
#define SHAKE_BURST_N 6
#define SHAKE_RELEASE_S 5

// Element radii relative to C_RADIUS. Each offset fixes that element's place
// on screen, which leaves C_RADIUS alone in control of the curvature: a smaller
// radius curves the scale more sharply and brings more hours into view.
#define C_MARK_MID (C_RADIUS + 62.0f)  // scale circle the marks hang from
#define C_NUM_GAP 10.0f                // radial gap from the number to the tick
#define R_MINC (C_RADIUS - 54.0f)      // minute circle radius
#define R_TICK_INNER (C_MARK_MID - C_HOUR_LEN)  // inner end of the hour mark
#define C_HAND_CLEAR 15.0f             // hand tip reach past the scale line
#define C_HAND_TIP_BACK 4.5f           // short hand stops this far inside it

// The pill is the only place set from a font, carrying letters in every
// language the watch speaks. Its face differs from the dial's on purpose: the
// dial face hangs its J below the baseline, where French JEUDI and the month
// abbreviations JAN / JUN / JUL would drop into the line below.
// DY is the distance from the top of the drawing box to the visual center of
// the glyphs, which prv_draw_text_centered() places on its anchor.
#define PILL_SMALL_FONT_RES RESOURCE_ID_FONT_MONTSERRAT_P24
#define PILL_SMALL_DY 15
#define PILL_BIG_FONT_RES RESOURCE_ID_FONT_MONTSERRAT_P30
#define PILL_BIG_DY 19

// The two lines are laid out on their cap boxes rather than on the font's line
// box, which carries an ascender and a leading the pill has no room for.
// Measured off the rendered glyphs; in pixels, like the fonts, and the same on
// both platforms because the pill scales to the same 54 px inside on each.
#define PILL_SMALL_CAP 17
#define PILL_BIG_CAP 21
#define PILL_ACCENT 6      // how far Á É Ä Û ... reach over the cap line
#define PILL_TAIL 5        // how far Q and Ç reach under the baseline
#define PILL_LINE_GAP 6    // weekday baseline to the cap line of the date
#define PILL_MARK_CLEAR 2  // left free where a mark hangs into that gap

// ==========================================================================
// Settings and state
// ==========================================================================

#define PERSIST_KEY_HAND_FULL 1
#define PERSIST_KEY_ACCENT 2
#define PERSIST_KEY_SHOW_MINUTE 3
#define PERSIST_KEY_PILL_BATTERY 4
#define PERSIST_KEY_FINE_TICKS 5   // no longer written; read to migrate to SCALE_STYLE
#define PERSIST_KEY_SHAKE_PILL 6
#define PERSIST_KEY_MINUTE_COLON 7
#define PERSIST_KEY_HAND_MODE 8
#define PERSIST_KEY_HOUR_SIZE 10  // no longer written; read to migrate to HOUR_SIZE_4
#define PERSIST_KEY_HOUR_SIZE_4 12
#define PERSIST_KEY_SCALE_STYLE 11
#define PERSIST_KEY_BIRTHDAY 13     // packed YYYYMMDD
#define PERSIST_KEY_GENDER 14
#define PERSIST_KEY_COUNTRY 15      // ISO 3166-1 alpha-2, persisted as a string
#define PERSIST_KEY_USE_LOCATION 16
#define PERSIST_KEY_PILL_LIFE 17

static Window *s_window;
static Layer *s_dial_layer, *s_pill_layer;
static GFont s_pill_small_font, s_pill_big_font;

static enum { PILL_HIDDEN, PILL_IN, PILL_SHOWN, PILL_OUT } s_pill_state;
static AppTimer *s_pill_timer;

// s_pill_reveal is the animated 0..1 size fraction the pill is drawn at,
// growing from its own center out to full size and shrinking back to it,
// rather than sliding the whole layer on and off the top edge of the screen.
// s_reveal_from and s_reveal_to are the ends of the running animation.
static float s_pill_reveal, s_reveal_from, s_reveal_to;
static Animation *s_reveal_anim;

// Pages the pill cycles through on repeated shakes, skipping any page whose
// toggle is off. Always opens on PAGE_DATE; see prv_pill_advance().
typedef enum {
  PAGE_DATE = 0,
  PAGE_BATTERY = 1,
  PAGE_LIFE = 2,
} PillPage;
static uint8_t s_pill_page;

// s_fill is the animated 0..1 fraction of the battery level drawn so far,
// s_fill_from and s_fill_to the ends of the running animation.
static float s_fill, s_fill_from, s_fill_to;
static Animation *s_fill_anim;
static bool s_was_plugged;      // last known plug state, to spot the transition
static bool s_charge_dismissed; // gauge shaken away for this charging session

// Hand shapes. 0 and 1 are what PERSIST_KEY_HAND_FULL held as a boolean, so
// that key migrates by being read as this enum.
typedef enum {
  HAND_NEEDLE = 0,  // fine tip reaching past the marks
  HAND_FULL = 1,    // bar across the whole dial
  HAND_SCALE = 2,   // tip stopping short of the scale circle
} HandMode;
static uint8_t s_hand_mode;
// Hour number sizes, one per bitmap cut and numbered to match the index into
// s_hour_cuts. The two upper sizes stand where the minute circle is, so
// prv_minute_visible() drops it for them.
typedef enum {
  HOUR_STD = 0,     // 24 px digits
  HOUR_LARGE = 1,   // 34 px, the largest that clears the minute circle
  HOUR_LARGER = 2,  // 42 px, no minute circle
  HOUR_HUGE = 3,    // 56 px, no minute circle
} HourSize;
static bool s_show_minute;   // draw the minute circle at all
static bool s_minute_colon; // prefix the minute with a colon, smaller type
static uint8_t s_hour_size; // HourSize: which hour cut is in use
static bool s_pill_battery;  // enable the battery page
static bool s_pill_life;     // enable the weeks-left page
static bool s_shake_pill;    // let a wrist shake bring the pill up at all
#if MOTION_MUTE_ENABLED
static time_t s_last_tap;    // for spotting sustained motion
static uint8_t s_tap_run;    // taps so far inside the burst window
static bool s_shake_muted;   // sustained motion seen, waiting for calm
#endif

// Weeks-left settings. s_use_location is informational only on the watch —
// the actual geolocation lookup happens in PebbleKit JS, which then sends
// COUNTRY same as a manual pick would.
static int s_birth_year = 1980, s_birth_month = 1, s_birth_day = 1;
static uint8_t s_gender;      // 0 male, 1 female, 2 other
static char s_country[8] = "WORLD";  // ISO 3166-1 alpha-2, or "WORLD"; upper-cased
static bool s_use_location;
// Three complete scales rather than two switches to combine. 0 and 1 match the
// boolean in PERSIST_KEY_FINE_TICKS, which migrates by being read as this enum.
typedef enum {
  SCALE_CLASSIC = 0,   // round-ended marks, a dot at ten minutes, nothing below
  SCALE_TENTHS = 1,    // square bars, 60 / 30 / 10, a dot at five
  SCALE_QUARTERS = 2,  // square bars, 60 / 30 / 15 / 5, all strokes
} ScaleStyle;
static uint8_t s_scale_style;
static GColor s_accent;      // hand and minute circle color

static int16_t s_w, s_h;
static float s_scale;
static float s_angle;       // hour hand angle in degrees
static GPoint s_rotc;       // scale rotation center for the current angle

// Full-length hand polygon in scaled screen units, rotated via GPath
static GPoint s_hand_points[4];
static GPath *s_hand_path;

// Square-ended scale bar, rewritten per mark and drawn through this one path
static GPoint s_bar_points[4];
static GPath *s_bar_path;

// Charging bolt polygon, likewise in scaled screen units
static GPoint s_bolt_points[6];
static GPath *s_bolt_path;

// The single time reading a frame is built from — hand angle, hours, minute
// and date all come out of this, so a frame cannot straddle a minute boundary.
// Refreshed on every tick and once at startup.
static struct tm s_now;

// ==========================================================================
// Geometry
// ==========================================================================

static void prv_update_time(void) {
  time_t t = time(NULL);
  s_now = *localtime(&t);
  s_angle = 30.0f * (s_now.tm_hour % 12) + s_now.tm_min / 2.0f;
}

// Rotates a point in reference units around the scale center, in screen pixels.
static GPoint prv_rot(float px, float py, float deg) {
  int32_t ta = DEG_TO_TRIGANGLE(deg);
  float sv = (float)sin_lookup(ta) / TRIG_MAX_RATIO;
  float cv = (float)cos_lookup(ta) / TRIG_MAX_RATIO;
  float x = px * s_scale, y = py * s_scale;
  return GPoint(s_rotc.x + (int16_t)(x * cv - y * sv),
                s_rotc.y + (int16_t)(x * sv + y * cv));
}

static void prv_set_rotc(void) {
  int32_t ta = DEG_TO_TRIGANGLE(s_angle);
  float r = C_RADIUS * s_scale;
  s_rotc = GPoint(s_w / 2 - (int16_t)(r * sin_lookup(ta) / TRIG_MAX_RATIO),
                  s_h / 2 + (int16_t)(r * cos_lookup(ta) / TRIG_MAX_RATIO));
}

// ==========================================================================
// Type
// ==========================================================================

// Draws text with its visual center on `center`; dy = measured distance
// from the drawing box top to the visual glyph center for that font. The box
// spans a full screen width either side of the anchor: a fixed width would
// clip the longest localized weekdays (DONNERSTAG is 222 px in the gabbro
// pill font), and off-screen parts of the box cost nothing.
static void prv_draw_text_centered(GContext *ctx, const char *str, GFont font,
                                   GPoint center, int16_t dy) {
  GRect box = GRect(center.x - s_w, center.y - dy, s_w * 2, dy * 2 + 10);
  graphics_draw_text(ctx, str, font, box, GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

// The dial numbers are blitted from bitmaps rather than set from a font, which
// is what makes them antialiased: the font pipeline rasters glyphs to one bit
// per pixel, while a bitmap carries the four levels the display can show.
//
// Six tables describe each cut: advance width, the two side bearings, the drop
// below the shared cap line, and the box width and height. There is no
// kerning, so the ink extent of a string follows from these alone. The
// bearings are signed.
//
// Generated by tools/make_digit_bitmaps.py — re-run it rather than editing.
// 34 ppem, digits 26 px tall
static const uint8_t s_b_adv34[10] = {21, 16, 21, 20, 21, 20, 21, 19, 20, 21};
static const int8_t s_b_lsb34[10] = {1, 2, 0, 1, 0, 1, 1, 1, 1, 1};
static const int8_t s_b_rsb34[10] = {1, 3, 2, 1, 0, 1, 1, 0, 2, 1};
static const uint8_t s_b_top34[10] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0};
static const uint8_t s_b_w34[10] = {19, 11, 19, 18, 21, 18, 19, 18, 17, 19};
static const uint8_t s_b_h34[10] = {26, 25, 25, 26, 24, 25, 25, 24, 26, 25};
#define BMP_CAP_34 24  // flat cap height, the optical band
#define BMP_MID_34 13   // top line down to the middle of it
// Ink mass of each digit, and the middle of that mass measured from the pen
// position in eighths of a pixel.
static const uint16_t s_b_mass34[10] = {656, 346, 597, 557, 531, 607, 595, 444, 668, 595};
static const int16_t s_b_cen34[10] = {85, 74, 87, 90, 96, 89, 83, 79, 79, 83};
// Reach towards the mark, by hour and then by digit, in pixels.
static const int8_t s_b_sup34[12][10] = {
  {  12,   12,   12,   12,   12,   12,   12,   12,   12,   12},  // 12 o'clock
  {  18,   16,   18,   18,   18,   20,   18,   20,   17,   18},  // 1 o'clock
  {  20,   16,   20,   19,   20,   22,   20,   22,   19,   20},  // 2 o'clock
  {  20,   12,   19,   18,   20,   19,   19,   19,   18,   19},  // 3 o'clock
  {  20,   16,   22,   20,   21,   20,   20,   13,   20,   16},  // 4 o'clock
  {  18,   16,   20,   18,   18,   18,   18,   14,   17,   15},  // 5 o'clock
  {  12,   12,   12,   12,   12,   12,   12,   12,   12,   12},  // 6 o'clock
  {   7,    6,   10,    8,    6,    8,    7,    9,    8,    8},  // 7 o'clock
  {   2,   -1,    5,    2,    3,    3,    2,    3,    2,    2},  // 8 o'clock
  {  -1,   -3,   -1,   -2,   -1,   -2,   -2,   -1,   -2,   -2},  // 9 o'clock
  {   2,    2,    2,    2,   -4,    1,   -2,    5,    2,    2},  // 10 o'clock
  {   7,    8,    7,    7,    3,    7,    5,   10,    7,    7},  // 11 o'clock
};
// 48 ppem, digits 36 px tall
static const uint8_t s_b_adv48[10] = {30, 23, 29, 28, 30, 29, 29, 27, 28, 29};
static const int8_t s_b_lsb48[10] = {1, 3, 0, 2, 0, 1, 2, 1, 2, 2};
static const int8_t s_b_rsb48[10] = {1, 6, 2, 2, 1, 2, 1, 0, 2, 1};
static const uint8_t s_b_top48[10] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0};
static const uint8_t s_b_w48[10] = {28, 14, 27, 24, 29, 26, 26, 26, 24, 26};
static const uint8_t s_b_h48[10] = {36, 35, 35, 36, 34, 35, 35, 34, 36, 35};
#define BMP_CAP_48 34  // flat cap height, the optical band
#define BMP_MID_48 18   // top line down to the middle of it
// Ink mass of each digit, and the middle of that mass measured from the pen
// position in eighths of a pixel.
static const uint16_t s_b_mass48[10] = {1314, 697, 1199, 1108, 1099, 1212, 1185, 898, 1338, 1184};
static const int16_t s_b_cen48[10] = {120, 104, 123, 128, 137, 126, 118, 110, 111, 118};
// Reach towards the mark, by hour and then by digit, in pixels.
static const int8_t s_b_sup48[12][10] = {
  {  18,   17,   18,   18,   17,   17,   17,   17,   18,   18},  // 12 o'clock
  {  25,   23,   25,   25,   26,   28,   26,   28,   24,   25},  // 1 o'clock
  {  28,   23,   28,   27,   28,   31,   28,   31,   27,   29},  // 2 o'clock
  {  28,   17,   27,   26,   29,   27,   27,   26,   25,   27},  // 3 o'clock
  {  28,   23,   32,   28,   30,   28,   29,   18,   27,   23},  // 4 o'clock
  {  25,   23,   28,   25,   26,   25,   25,   20,   24,   22},  // 5 o'clock
  {  18,   17,   17,   18,   17,   18,   18,   17,   18,   17},  // 6 o'clock
  {  10,    9,   14,   10,    8,   11,   10,   12,   10,   11},  // 7 o'clock
  {   2,   -1,    8,    3,    3,    4,    3,    4,    3,    2},  // 8 o'clock
  {  -2,   -4,   -1,   -3,   -2,   -2,   -2,   -1,   -3,   -2},  // 9 o'clock
  {   2,    4,    3,    2,   -6,    1,   -3,    8,    3,    3},  // 10 o'clock
  {  10,   11,   10,   10,    5,   10,    7,   14,   10,   10},  // 11 o'clock
};
// 60 ppem, digits 44 px tall
static const uint8_t s_b_adv60[10] = {38, 28, 36, 35, 38, 36, 37, 33, 35, 37};
static const int8_t s_b_lsb60[10] = {2, 4, 1, 3, 1, 2, 3, 1, 3, 3};
static const int8_t s_b_rsb60[10] = {2, 6, 2, 2, 2, 2, 3, -1, 3, 3};
static const uint8_t s_b_top60[10] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0};
static const uint8_t s_b_w60[10] = {34, 18, 33, 30, 35, 32, 31, 33, 29, 31};
static const uint8_t s_b_h60[10] = {44, 43, 43, 44, 42, 43, 43, 42, 44, 43};
#define BMP_CAP_60 42  // flat cap height, the optical band
#define BMP_MID_60 22   // top line down to the middle of it
// Ink mass of each digit, and the middle of that mass measured from the pen
// position in eighths of a pixel.
static const uint16_t s_b_mass60[10] = {2034, 1071, 1849, 1717, 1682, 1877, 1823, 1388, 2076, 1821};
static const int16_t s_b_cen60[10] = {150, 130, 154, 160, 169, 158, 147, 138, 139, 147};
// Reach towards the mark, by hour and then by digit, in pixels.
static const int8_t s_b_sup60[12][10] = {
  {  22,   21,   22,   22,   21,   21,   21,   21,   22,   22},  // 12 o'clock
  {  31,   29,   31,   30,   33,   35,   32,   35,   30,   31},  // 1 o'clock
  {  35,   29,   35,   34,   36,   39,   35,   39,   33,   35},  // 2 o'clock
  {  35,   21,   33,   32,   36,   33,   34,   33,   31,   34},  // 3 o'clock
  {  35,   29,   39,   35,   38,   35,   35,   22,   34,   28},  // 4 o'clock
  {  31,   29,   35,   31,   33,   31,   31,   25,   30,   27},  // 5 o'clock
  {  22,   21,   21,   22,   21,   22,   22,   21,   22,   21},  // 6 o'clock
  {  12,   11,   17,   13,   11,   13,   12,   15,   13,   14},  // 7 o'clock
  {   2,   -2,    9,    4,    6,    6,    4,    5,    4,    4},  // 8 o'clock
  {  -2,   -4,   -2,   -3,   -1,   -2,   -3,   -2,   -3,   -3},  // 9 o'clock
  {   2,    5,    3,    3,   -7,    1,   -3,    9,    3,    4},  // 10 o'clock
  {  12,   13,   12,   12,    6,   13,    8,   17,   13,   12},  // 11 o'clock
};
// 80 ppem, digits 59 px tall
static const uint8_t s_b_adv80[10] = {50, 38, 48, 46, 50, 48, 49, 44, 46, 49};
static const int8_t s_b_lsb80[10] = {3, 6, 1, 4, 1, 2, 4, 2, 4, 4};
static const int8_t s_b_rsb80[10] = {3, 9, 3, 3, 2, 3, 3, -1, 4, 3};
static const uint8_t s_b_top80[10] = {1, 1, 1, 1, 2, 2, 2, 2, 0, 0};
static const uint8_t s_b_w80[10] = {44, 23, 44, 39, 47, 43, 42, 43, 38, 42};
static const uint8_t s_b_h80[10] = {58, 57, 57, 58, 56, 57, 57, 56, 59, 58};
#define BMP_CAP_80 56  // flat cap height, the optical band
#define BMP_MID_80 30   // top line down to the middle of it
// Ink mass of each digit, and the middle of that mass measured from the pen
// position in eighths of a pixel.
static const uint16_t s_b_mass80[10] = {3614, 1900, 3266, 3054, 3002, 3333, 3244, 2476, 3679, 3246};
static const int16_t s_b_cen80[10] = {200, 174, 205, 212, 226, 210, 197, 184, 185, 196};
// Reach towards the mark, by hour and then by digit, in pixels.
static const int8_t s_b_sup80[12][10] = {
  {  29,   29,   29,   29,   28,   28,   28,   28,   29,   29},  // 12 o'clock
  {  41,   39,   41,   40,   44,   46,   43,   46,   40,   41},  // 1 o'clock
  {  46,   39,   46,   45,   48,   52,   47,   52,   44,   47},  // 2 o'clock
  {  47,   28,   44,   43,   48,   45,   45,   44,   42,   45},  // 3 o'clock
  {  46,   38,   52,   46,   50,   47,   47,   30,   45,   38},  // 4 o'clock
  {  41,   38,   46,   41,   44,   41,   41,   33,   40,   36},  // 5 o'clock
  {  29,   28,   28,   29,   28,   29,   29,   28,   29,   28},  // 6 o'clock
  {  16,   15,   23,   17,   14,   18,   17,   21,   17,   19},  // 7 o'clock
  {   3,   -2,   12,    5,    7,    7,    4,    8,    5,    4},  // 8 o'clock
  {  -3,   -6,   -2,   -4,   -2,   -3,   -4,   -2,   -4,   -4},  // 9 o'clock
  {   3,    7,    4,    3,  -10,    1,   -5,   12,    4,    4},  // 10 o'clock
  {  16,   18,   16,   16,    8,   17,   11,   23,   17,   17},  // 11 o'clock
};

// Cuts for the minute in the circle. Index 10 of the 25 ppem set is the colon,
// which hangs well below the cap line the digits share.
// 25 ppem, digits 20 px tall, index 10 is the colon
static const uint8_t s_m_adv25[11] = {16, 12, 15, 14, 16, 15, 15, 14, 14, 15, 8};
static const int8_t s_m_lsb25[11] = {1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 2};
static const int8_t s_m_rsb25[11] = {1, 3, 1, 0, 1, 1, 0, 0, 0, 0, 2};
static const uint8_t s_m_top25[11] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 6};
static const uint8_t s_m_w25[11] = {14, 8, 14, 13, 15, 14, 14, 14, 13, 14, 4};
static const uint8_t s_m_h25[11] = {20, 19, 19, 20, 18, 19, 19, 18, 20, 19, 14};
#define BMP_CAP_25 18  // flat cap height, the optical band
#define BMP_MID_25 10   // top line down to the middle of it
// 30 ppem, digits 23 px tall
static const uint8_t s_m_adv30[10] = {19, 14, 18, 17, 19, 18, 18, 17, 17, 18};
static const int8_t s_m_lsb30[10] = {1, 2, 0, 1, 0, 1, 1, 0, 1, 1};
static const int8_t s_m_rsb30[10] = {1, 3, 1, 0, 1, 1, 1, 0, 1, 1};
static const uint8_t s_m_top30[10] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0};
static const uint8_t s_m_w30[10] = {17, 9, 17, 16, 18, 16, 16, 17, 15, 16};
static const uint8_t s_m_h30[10] = {23, 22, 22, 23, 21, 22, 22, 21, 23, 22};
#define BMP_CAP_30 21  // flat cap height, the optical band
#define BMP_MID_30 11   // top line down to the middle of it
// Deliberate one-pixel offset, not a rounding error: leave it in. Geometrically
// the minute is centered in its circle and still reads as sitting low, which is
// what centering inside a closed shape does to the eye. The hour numbers get no
// such lift, having no shape around them to be judged against.
#define MIN_OPTICAL_LIFT 1

static GBitmap *s_digit_bmp[10];
static GBitmap *s_min_bmp[11];

// One cut gathered into a single value. The drawing routines take a Cut and
// never learn which one it is, so the hours and the minute share the same code
// and a further size is one more row in a table.
typedef struct {
  const uint8_t *adv, *top, *w, *h;
  const int8_t *lsb, *rsb;
  uint8_t cap;  // flat cap height, the optical band
  uint8_t mid;  // top line down to the middle of that band
  // Reach of each glyph towards each of the twelve hour marks. NULL on the
  // minute, which sits in its circle rather than against a mark.
  const int8_t (*sup)[10];
  // Ink mass of each glyph and the middle of that mass, in eighths of a pixel
  // from the pen. Set only where a number stands free against a mark; see
  // prv_run_pen() for why the minute goes without.
  const uint16_t *mass;
  const int16_t *cen;
} Cut;

// Indexed by HourSize, which is numbered to match.
static const Cut s_hour_cuts[4] = {
  {s_b_adv34, s_b_top34, s_b_w34, s_b_h34, s_b_lsb34, s_b_rsb34, BMP_CAP_34, BMP_MID_34, s_b_sup34,
   s_b_mass34, s_b_cen34},
  {s_b_adv48, s_b_top48, s_b_w48, s_b_h48, s_b_lsb48, s_b_rsb48, BMP_CAP_48, BMP_MID_48, s_b_sup48,
   s_b_mass48, s_b_cen48},
  {s_b_adv60, s_b_top60, s_b_w60, s_b_h60, s_b_lsb60, s_b_rsb60, BMP_CAP_60, BMP_MID_60, s_b_sup60,
   s_b_mass60, s_b_cen60},
  {s_b_adv80, s_b_top80, s_b_w80, s_b_h80, s_b_lsb80, s_b_rsb80, BMP_CAP_80, BMP_MID_80, s_b_sup80,
   s_b_mass80, s_b_cen80},
};
// Index 0 is the cut with the colon, index 1 the one without.
static const Cut s_min_cuts[2] = {
  {s_m_adv25, s_m_top25, s_m_w25, s_m_h25, s_m_lsb25, s_m_rsb25, BMP_CAP_25, BMP_MID_25, NULL, NULL, NULL},
  {s_m_adv30, s_m_top30, s_m_w30, s_m_h30, s_m_lsb30, s_m_rsb30, BMP_CAP_30, BMP_MID_30, NULL, NULL, NULL},
};
// The cuts in use, so the drawing code stays free of the settings
static const Cut *s_hcut = &s_hour_cuts[0];
static const Cut *s_mcut = &s_min_cuts[0];

// Digit images by cut and then by digit. Resource ids are macros, so they have
// to be spelled out.
static const uint32_t s_digit_res[4][10] = {
  {RESOURCE_ID_IMAGE_DIGIT_34_0, RESOURCE_ID_IMAGE_DIGIT_34_1,
   RESOURCE_ID_IMAGE_DIGIT_34_2, RESOURCE_ID_IMAGE_DIGIT_34_3,
   RESOURCE_ID_IMAGE_DIGIT_34_4, RESOURCE_ID_IMAGE_DIGIT_34_5,
   RESOURCE_ID_IMAGE_DIGIT_34_6, RESOURCE_ID_IMAGE_DIGIT_34_7,
   RESOURCE_ID_IMAGE_DIGIT_34_8, RESOURCE_ID_IMAGE_DIGIT_34_9},
  {RESOURCE_ID_IMAGE_DIGIT_48_0, RESOURCE_ID_IMAGE_DIGIT_48_1,
   RESOURCE_ID_IMAGE_DIGIT_48_2, RESOURCE_ID_IMAGE_DIGIT_48_3,
   RESOURCE_ID_IMAGE_DIGIT_48_4, RESOURCE_ID_IMAGE_DIGIT_48_5,
   RESOURCE_ID_IMAGE_DIGIT_48_6, RESOURCE_ID_IMAGE_DIGIT_48_7,
   RESOURCE_ID_IMAGE_DIGIT_48_8, RESOURCE_ID_IMAGE_DIGIT_48_9},
  {RESOURCE_ID_IMAGE_DIGIT_60_0, RESOURCE_ID_IMAGE_DIGIT_60_1,
   RESOURCE_ID_IMAGE_DIGIT_60_2, RESOURCE_ID_IMAGE_DIGIT_60_3,
   RESOURCE_ID_IMAGE_DIGIT_60_4, RESOURCE_ID_IMAGE_DIGIT_60_5,
   RESOURCE_ID_IMAGE_DIGIT_60_6, RESOURCE_ID_IMAGE_DIGIT_60_7,
   RESOURCE_ID_IMAGE_DIGIT_60_8, RESOURCE_ID_IMAGE_DIGIT_60_9},
  {RESOURCE_ID_IMAGE_DIGIT_80_0, RESOURCE_ID_IMAGE_DIGIT_80_1,
   RESOURCE_ID_IMAGE_DIGIT_80_2, RESOURCE_ID_IMAGE_DIGIT_80_3,
   RESOURCE_ID_IMAGE_DIGIT_80_4, RESOURCE_ID_IMAGE_DIGIT_80_5,
   RESOURCE_ID_IMAGE_DIGIT_80_6, RESOURCE_ID_IMAGE_DIGIT_80_7,
   RESOURCE_ID_IMAGE_DIGIT_80_8, RESOURCE_ID_IMAGE_DIGIT_80_9},
};

/**
 * Puts the chosen hour cut in place: its ten images and their metrics. Only one
 * cut is ever resident, the largest costing about 6 KB of heap.
 *
 * The sizes stop where they do because an hour number grows sideways as well as
 * inwards and shares its angle with the minute circle near the top of the hour.
 * HOUR_LARGE is the largest that clears the circle on both 12- and 24-hour
 * dials; the sizes above it drop the circle and are limited by the neighbouring
 * hour instead. Changing a cut size means re-checking both clearances over
 * every hour and minute, in screen coordinates — ink boxes are axis-aligned, so
 * the gap between two of them changes as the scale rotates.
 */
static void prv_apply_hour_cut(void) {
  // Out-of-range settings fall back to the smallest cut rather than indexing
  // past the table.
  unsigned cut = s_hour_size <= HOUR_HUGE ? s_hour_size : HOUR_STD;
  s_hcut = &s_hour_cuts[cut];
  for (unsigned d = 0; d < 10; d++) {
    gbitmap_destroy(s_digit_bmp[d]);  // NULL on the first call, which is allowed
    s_digit_bmp[d] = gbitmap_create_with_resource(s_digit_res[cut][d]);
  }
}

static const uint32_t s_min_res[2][11] = {
  {RESOURCE_ID_IMAGE_DIGIT_25_0, RESOURCE_ID_IMAGE_DIGIT_25_1,
   RESOURCE_ID_IMAGE_DIGIT_25_2, RESOURCE_ID_IMAGE_DIGIT_25_3,
   RESOURCE_ID_IMAGE_DIGIT_25_4, RESOURCE_ID_IMAGE_DIGIT_25_5,
   RESOURCE_ID_IMAGE_DIGIT_25_6, RESOURCE_ID_IMAGE_DIGIT_25_7,
   RESOURCE_ID_IMAGE_DIGIT_25_8, RESOURCE_ID_IMAGE_DIGIT_25_9,
   RESOURCE_ID_IMAGE_DIGIT_25_10},
  {RESOURCE_ID_IMAGE_DIGIT_30_0, RESOURCE_ID_IMAGE_DIGIT_30_1,
   RESOURCE_ID_IMAGE_DIGIT_30_2, RESOURCE_ID_IMAGE_DIGIT_30_3,
   RESOURCE_ID_IMAGE_DIGIT_30_4, RESOURCE_ID_IMAGE_DIGIT_30_5,
   RESOURCE_ID_IMAGE_DIGIT_30_6, RESOURCE_ID_IMAGE_DIGIT_30_7,
   RESOURCE_ID_IMAGE_DIGIT_30_8, RESOURCE_ID_IMAGE_DIGIT_30_9, 0},
};

// The colon setting picks the cut and with it the number of resident images:
// eleven with the colon, ten without. The unused slot stays NULL.
static void prv_apply_minute_cut(void) {
  unsigned cut = s_minute_colon ? 0 : 1;
  unsigned n = s_minute_colon ? 11 : 10;
  s_mcut = &s_min_cuts[cut];
  for (unsigned d = 0; d < 11; d++) {
    gbitmap_destroy(s_min_bmp[d]);
    s_min_bmp[d] = d < n ? gbitmap_create_with_resource(s_min_res[cut][d]) : NULL;
  }
}

// Ink edges of a run of glyphs, measured from the pen position.
static void prv_run_ink(const Cut *cut, const uint8_t *ix, unsigned n,
                        int16_t *ink_l, int16_t *ink_r) {
  int16_t adv = 0;
  for (unsigned i = 0; i < n; i++) {
    adv += cut->adv[ix[i]];
  }
  *ink_l = cut->lsb[ix[0]];
  *ink_r = adv - cut->rsb[ix[n - 1]];
}

/**
 * Pen offset from the anchor, so that the run comes out centred on it.
 *
 * Two rules, because two situations. A number standing free beside a mark is
 * centred on its **ink mass**: a "1" carries nearly all of its ink in the stem,
 * right of the middle of its box, and centring the box leaves the stem beside
 * the mark instead of on it. A number inside a closed shape is centred on its
 * **ink box** instead, because there the eye reads the air either side of the
 * digits rather than their weight — measured on the two worst minutes, mass
 * centring left the ring three pixels fuller on one side than the other, where
 * the box keeps it even to within one.
 *
 * A run of odd ink width cannot sit centred on the pixel grid either way, so
 * the half pixel has to fall on the same side as whatever the run is centred
 * against. A mark or a circle is drawn from a point that names a pixel, which
 * puts its own middle half a pixel to the right of it; truncating here does the
 * same for the run.
 */
static int16_t prv_run_pen(const Cut *cut, const uint8_t *ix, unsigned n) {
  if (cut->cen) {
    uint32_t m = 0;
    float sx = 0.0f;
    int16_t p = 0;
    for (unsigned i = 0; i < n; i++) {
      uint8_t d = ix[i];
      m += cut->mass[d];
      sx += cut->mass[d] * (p + cut->cen[d] / 8.0f);
      p += cut->adv[d];
    }
    return -(int16_t)(sx / m);
  }
  int16_t ink_l, ink_r;
  prv_run_ink(cut, ix, n, &ink_l, &ink_r);
  return -(int16_t)((ink_l + ink_r) / 2.0f);
}

/**
 * Blits a run of glyphs centred on `center` and raised by `lift` pixels. What
 * sits on the anchor is the ink, not the advance width: centering the latter
 * would put an asymmetric digit a pixel and a half off. The cap line sits
 * cut->mid above the anchor and each glyph hangs from it by its own offset, so
 * the round digits keep their overshoot.
 */
static void prv_blit_run(GContext *ctx, const Cut *cut, GBitmap *const *bmp,
                         const uint8_t *ix, unsigned n, GPoint center,
                         int16_t lift) {
  int16_t pen = center.x + prv_run_pen(cut, ix, n);
  int16_t cap = center.y - cut->mid - lift;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  for (unsigned i = 0; i < n; i++) {
    uint8_t d = ix[i];
    graphics_draw_bitmap_in_rect(
        ctx, bmp[d], GRect(pen + cut->lsb[d], cap + cut->top[d],
                           cut->w[d], cut->h[d]));
    pen += cut->adv[d];
  }
}

// ==========================================================================
// Dial
// ==========================================================================

// The two upper hour sizes occupy the space the circle needs, so they suppress
// it. s_show_minute is left alone, so the circle returns with a smaller size.
static bool prv_minute_visible(void) {
  return s_show_minute && s_hour_size < HOUR_LARGER;
}

// Where the needle ends. The short tip stops C_HAND_TIP_BACK inside the scale
// circle rather than on it, so the outer end of the mark it stands on stays
// visible and the hand indicates that mark instead of covering it.
static float prv_hand_tip(void) {
  return s_hand_mode == HAND_SCALE
      ? C_MARK_MID - C_HAND_TIP_BACK
      : C_MARK_MID + C_HAND_CLEAR;
}

/**
 * Horizontal part of the direction each hour number faces, indexed by hour. The
 * vertical part is already contained in the reach tables.
 */
static const float s_hour_dir_x[12] = {
  0.0f, 0.5f, 0.8660254f, 1.0f, 0.8660254f, 0.5f,
  0.0f, -0.5f, -0.8660254f, -1.0f, -0.8660254f, -0.5f,
};

/**
 * Radius that leaves a constant gap C_NUM_GAP between the ink of an hour number
 * and the inner end of its mark. A fixed radius would make the gap depend on
 * the digits and the angle, so the reach of the number towards its mark is
 * subtracted from it.
 *
 * That reach is measured on the ink, per glyph and per mark, by
 * make_digit_bitmaps.py. Estimating it from the bounding box instead costs
 * accuracy where the box is not full: a "7" leaves its bottom left empty, and
 * at five o'clock that is the corner facing the mark, which held the number
 * more than two units short of the gap it was supposed to keep.
 *
 * Shifting a glyph along the pen line adds the shift projected onto the
 * direction, so the reach of a run is the largest of its glyphs'.
 */
static float prv_num_radius(unsigned hour, const uint8_t *ix, unsigned n) {
  float ux = s_hour_dir_x[hour];
  // The same pen the blit will use, so the reach is measured where the ink
  // actually lands.
  float pen = prv_run_pen(s_hcut, ix, n);
  float reach = pen * ux + s_hcut->sup[hour][ix[0]];
  for (unsigned i = 1; i < n; i++) {
    pen += s_hcut->adv[ix[i - 1]];
    float v = pen * ux + s_hcut->sup[hour][ix[i]];
    if (v > reach) {
      reach = v;
    }
  }
  return R_TICK_INNER - C_NUM_GAP - reach / s_scale;
}

// One mark of the scale: a round-capped stroke hanging inside C_MARK_MID. The
// endpoints are inset by the cap radius so the visible length comes out as
// `len` whatever the width. Off-screen marks are discarded here.
static void prv_draw_mark(GContext *ctx, float deg, float len, float width) {
  GPoint a = prv_rot(0, -(C_MARK_MID - len + width / 2), deg);
  GPoint b = prv_rot(0, -(C_MARK_MID - width / 2), deg);
  int16_t m = (int16_t)(width * s_scale) + 1;
  if ((a.x < -m && b.x < -m) || (a.x > s_w + m && b.x > s_w + m) ||
      (a.y < -m && b.y < -m) || (a.y > s_h + m && b.y > s_h + m)) {
    return;
  }
  graphics_context_set_stroke_width(ctx, (uint8_t)(width * s_scale + 0.5f));
  graphics_draw_line(ctx, a, b);
}

// A mark with square ends, for the hour and half hour of the ruled scales.
// graphics_draw_line() caps a wide stroke with a semicircle, which at this
// weight turns the bar into a pill, so it is drawn as a filled quad instead.
// s_bar_path is reused for every mark: the points are rewritten in place and
// the path itself is never rotated or moved.
static void prv_draw_bar(GContext *ctx, float deg, float len, float width) {
  float hw = width / 2;
  s_bar_points[0] = prv_rot(-hw, -(C_MARK_MID - len), deg);
  s_bar_points[1] = prv_rot(hw, -(C_MARK_MID - len), deg);
  s_bar_points[2] = prv_rot(hw, -C_MARK_MID, deg);
  s_bar_points[3] = prv_rot(-hw, -C_MARK_MID, deg);
  int16_t lo_x = s_bar_points[0].x, hi_x = lo_x, lo_y = s_bar_points[0].y, hi_y = lo_y;
  for (unsigned i = 1; i < 4; i++) {
    GPoint p = s_bar_points[i];
    if (p.x < lo_x) lo_x = p.x;
    if (p.x > hi_x) hi_x = p.x;
    if (p.y < lo_y) lo_y = p.y;
    if (p.y > hi_y) hi_y = p.y;
  }
  if (hi_x < 0 || lo_x > s_w || hi_y < 0 || lo_y > s_h) {
    return;
  }
  gpath_draw_filled(ctx, s_bar_path);
}

// One dot of the scale, hung inside C_MARK_MID so dots and marks share an outer
// edge. A filled circle spans 2r+1 px, so the radius is derived from the stroke
// width the dot stands in for: rounding the two independently would make the
// dots visibly fatter than the marks.
static void prv_draw_dot(GContext *ctx, float deg, float width) {
  GPoint p = prv_rot(0, -(C_MARK_MID - width / 2), deg);
  int16_t m = (int16_t)(width * s_scale) + 1;
  if (p.x < -m || p.x > s_w + m || p.y < -m || p.y > s_h + m) {
    return;
  }
  graphics_fill_circle(ctx, p, (uint16_t)((width * s_scale - 1.0f) / 2.0f + 0.5f));
}

// Draws one hour of the scale with its subdivisions and number. `raw` is an
// unbounded hour index, so the caller can ask for hours either side of the
// current one without wrapping them itself.
static void prv_draw_hour(GContext *ctx, int raw) {
  bool h24 = clock_is_24h_style();
  int disp = h24 ? ((raw % 24) + 24) % 24 : ((raw - 1) % 12 + 12) % 12 + 1;
  float a = 30.0f * raw;
  // Half a degree per minute, so every offset below is minutes / 2.
  static const float ten_offsets[] = {5, 10, 20, 25};          // 10, 20, 40, 50
  static const float dot5_offsets[] = {2.5f, 7.5f, 12.5f, 17.5f, 22.5f, 27.5f};
  static const float quarter_offsets[] = {7.5f, 22.5f};        // 15 and 45
  static const float five_offsets[] = {2.5f, 5, 10, 12.5f, 17.5f, 20, 25, 27.5f};

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  if (s_scale_style != SCALE_CLASSIC) {
    // On the ruled scales the hour and half hour lead by weight as well as
    // length: needle width, square ends.
    prv_draw_bar(ctx, a, C_HOUR_LEN, C_FINE_HOUR_W);
    prv_draw_bar(ctx, a + 15, C_HOUR_SLEN, C_FINE_HOUR_W);
    if (s_scale_style == SCALE_QUARTERS) {
      // 60 / 30 / 15 / 5, every step a stroke and shorter than the one above.
      for (unsigned i = 0; i < 2; i++) {
        prv_draw_mark(ctx, a + quarter_offsets[i], C_QUARTER_LEN, C_FINE_W);
      }
      for (unsigned i = 0; i < 8; i++) {
        prv_draw_mark(ctx, a + five_offsets[i], C_FIVE_LEN, C_FINE_W);
      }
    } else {
      // 60 / 30 / 10 with a dot at five: ten does not divide fifteen, so the
      // finest step here has to change shape rather than only length.
      for (unsigned i = 0; i < 4; i++) {
        prv_draw_mark(ctx, a + ten_offsets[i], C_FINE_TEN_LEN, C_FINE_W);
      }
      for (unsigned i = 0; i < 6; i++) {
        prv_draw_dot(ctx, a + dot5_offsets[i], C_DOT5_W);
      }
    }
  } else {
    // Classic: round-capped marks, dots at ten minutes, nothing finer.
    prv_draw_mark(ctx, a, C_HOUR_LEN, C_HOUR_W);
    prv_draw_mark(ctx, a + 15, C_HOUR_SLEN, C_HOUR_W);
    for (unsigned i = 0; i < 4; i++) {
      prv_draw_dot(ctx, a + ten_offsets[i], C_HOUR_W);
    }
  }

  char buf[4];
  snprintf(buf, sizeof(buf), "%d", disp);
  uint8_t ix[3];
  unsigned n = 0;
  for (const char *c = buf; *c; c++) {
    ix[n++] = *c - '0';
  }
  // Every hour faces one of twelve directions, so the mark itself is the index
  // into the reach tables.
  unsigned dir = (unsigned)(((raw % 12) + 12) % 12);
  GPoint np = prv_rot(0, -prv_num_radius(dir, ix, n), a);
  // Lift 0: see MIN_OPTICAL_LIFT, which applies to the minute only.
  prv_blit_run(ctx, s_hcut, s_digit_bmp, ix, n, np, 0);
}

// Draws the minute inside the ring riding on the hand. The ring is two filled
// circles, the inner one in the background color.
static void prv_draw_minute_circle(GContext *ctx) {
  GPoint mc = prv_rot(0, -R_MINC, s_angle);
  // Ring thickness stays at the needle weight regardless of hand style
  float half_hand = C_HAND_W / 2;
  uint16_t r_out = (uint16_t)((C_NUM_SIZE + half_hand) * s_scale + 0.5f);
  uint16_t r_in = (uint16_t)((C_NUM_SIZE - half_hand) * s_scale + 0.5f);

  graphics_context_set_fill_color(ctx, s_accent);
  graphics_fill_circle(ctx, mc, r_out);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, mc, r_in);

  // The colon is index 10 of its cut, so the run is built from glyph indices.
  uint8_t ix[3];
  unsigned n = 0;
  if (s_minute_colon) {
    ix[n++] = 10;
  }
  ix[n++] = s_now.tm_min / 10;
  ix[n++] = s_now.tm_min % 10;
  prv_blit_run(ctx, s_mcut, s_min_bmp, ix, n, mc, MIN_OPTICAL_LIFT);
}

static void prv_dial_draw(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, true);

  for (int i = -2; i <= 2; i++) {
    prv_draw_hour(ctx, s_now.tm_hour + i);
  }

  // Hand on top of the scale
  if (s_hand_mode == HAND_FULL) {
    graphics_context_set_fill_color(ctx, s_accent);
    gpath_rotate_to(s_hand_path, DEG_TO_TRIGANGLE(s_angle));
    gpath_move_to(s_hand_path, GPoint(s_w / 2, s_h / 2));
    gpath_draw_filled(ctx, s_hand_path);
  } else {
    // Endpoint inset by the cap radius so the visible tip lands on the radius
    // prv_hand_tip() returns. The width is truncated rather than rounded: a
    // round cap already reads heavier than a polygon of the same width.
    graphics_context_set_stroke_color(ctx, s_accent);
    graphics_context_set_stroke_width(ctx, (uint8_t)(C_HAND_W * s_scale));
    graphics_draw_line(ctx, s_rotc,
                       prv_rot(0, -(prv_hand_tip() - C_HAND_W / 2), s_angle));
  }

  // Suppressed while the pill is up, so no half-covered ring shows behind it.
  if (prv_minute_visible() && s_pill_state == PILL_HIDDEN) {
    prv_draw_minute_circle(ctx);
  }
}

// ==========================================================================
// Date pill
// ==========================================================================

// Uppercases in place, including the UTF-8 Latin-1 supplement (ä→Ä), which
// localized strftime output contains and toupper() alone would leave.
static void prv_upcase(char *s) {
  for (unsigned char *c = (unsigned char *)s; *c; c++) {
    if (*c == 0xC3 && c[1] >= 0xA0 && c[1] <= 0xBE && c[1] != 0xB7) {
      c[1] -= 0x20;
      c++;
    } else {
      *c = toupper(*c);
    }
  }
}

// True if the line reaches above the cap line. After prv_upcase() the only
// letters that do are the Latin-1 supplement ones, which all lead with 0xC3 —
// bar the cedilla and the slashed O, whose marks stay inside the cap. An
// accent this does not know about counts as tall, which is the safe way round.
static bool prv_is_tall(const char *s) {
  for (const unsigned char *c = (const unsigned char *)s; c[0] && c[1]; c++) {
    if (c[0] == 0xC3 && c[1] != 0x87 && c[1] != 0x98) {
      return true;
    }
  }
  return false;
}

// And true if it hangs under the baseline, which in this face is the cedilla
// and the tail of the Q. Portuguese brings both, in TERÇA and in QUARTA.
static bool prv_is_deep(const char *s) {
  for (const unsigned char *c = (const unsigned char *)s; c[0]; c++) {
    if (c[0] == 'Q' || (c[0] == 0xC3 && c[1] == 0x87)) {
      return true;
    }
  }
  return false;
}

// Both flags are needed: is_charging drops to false once the watch is full.
static bool prv_is_plugged(BatteryChargeState c) {
  return c.is_charging || c.is_plugged;
}

// Traffic-light coding. The darker shades keep the white percentage readable.
// The steps sit low because the watch runs for weeks: half a charge is days of
// wear away from the charger, and a colour that warns half the time is one the
// eye stops reading.
static GColor prv_batt_color(int pct) {
  if (pct <= 10) {
    return GColorDarkCandyAppleRed;
  }
  if (pct <= 25) {
    return GColorChromeYellow;  // GColorOrange reads as another red next to the accent
  }
  return GColorIslamicGreen;
}

// Weeks remaining between now and (birthday + life expectancy for
// s_country/s_gender), floored at zero. life_expectancy_years() falls back
// to a world average for an unrecognized or unset country.
static long prv_weeks_left(void) {
  struct tm birth = { 0 };
  birth.tm_year = s_birth_year - 1900;
  birth.tm_mon = s_birth_month - 1;
  birth.tm_mday = s_birth_day;
  birth.tm_hour = 12;  // noon, clear of any DST edge on the birth date itself
  time_t birth_ts = mktime(&birth);
  double weeks_lived = difftime(time(NULL), birth_ts) / 604800.0;
  double weeks_total = (double)life_expectancy_years(s_country, s_gender) * 52.1775;
  double left = weeks_total - weeks_lived;
  return (long)(left > 0 ? left + 0.5 : 0);
}

// Comma-groups a value for display, e.g. 12345 -> "12,345". A life in weeks
// never reaches five digits before the comma, so `out` need only be a dozen
// bytes; longer inputs are simply truncated rather than overflowing it.
static void prv_format_thousands(char *out, size_t n, long value) {
  char digits[16];
  snprintf(digits, sizeof(digits), "%ld", value);
  size_t len = strlen(digits);
  size_t oi = 0;
  for (size_t i = 0; i < len && oi + 1 < n; i++) {
    if (i > 0 && (len - i) % 3 == 0) {
      out[oi++] = ',';
      if (oi + 1 >= n) break;
    }
    out[oi++] = digits[i];
  }
  out[oi] = '\0';
}

/**
 * Draws the overlay pill on its own layer, which is what lets it grow and
 * shrink independently of the dial underneath. On the date and weeks-left
 * pages a small label sits above a big value, both in the watch's language
 * for the date, plain English for weeks-left. On the battery page the shell
 * keeps its size, width included so it does not jump between pages, and the
 * inside becomes a gauge.
 */
static void prv_pill_draw(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, true);
  char wday[24], date[16];
  strftime(wday, sizeof(wday), "%A", &s_now);
  strftime(date, sizeof(date), "%d %b", &s_now);
  prv_upcase(wday);
  prv_upcase(date);

  const char *weeks_label = "WEEKS LEFT";
  char weeks_value[12];
  prv_format_thousands(weeks_value, sizeof(weeks_value), prv_weeks_left());

  const char *small = s_pill_page == PAGE_LIFE ? weeks_label : wday;
  const char *big = s_pill_page == PAGE_LIFE ? weeks_value : date;

  // Width sized off both the date and the weeks-left text, whichever pair is
  // wider, so the pill does not change size when flipping between pages.
  GRect probe = GRect(0, 0, s_w, 60);
  GSize small_sz = graphics_text_layout_get_content_size(
      wday, s_pill_small_font, probe, GTextOverflowModeFill, GTextAlignmentCenter);
  GSize big_sz = graphics_text_layout_get_content_size(
      date, s_pill_big_font, probe, GTextOverflowModeFill, GTextAlignmentCenter);
  GSize life_small_sz = graphics_text_layout_get_content_size(
      weeks_label, s_pill_small_font, probe, GTextOverflowModeFill, GTextAlignmentCenter);
  GSize life_big_sz = graphics_text_layout_get_content_size(
      weeks_value, s_pill_big_font, probe, GTextOverflowModeFill, GTextAlignmentCenter);
  int16_t widest = small_sz.w;
  if (big_sz.w > widest) widest = big_sz.w;
  if (life_small_sz.w > widest) widest = life_small_sz.w;
  if (life_big_sz.w > widest) widest = life_big_sz.w;

  GPoint c = GPoint(s_w / 2, s_h / 2);
  int16_t half_w = widest / 2 + (int16_t)(C_PILL_PAD * s_scale);
  if (half_w > s_w / 2 - 2) {
    half_w = s_w / 2 - 2;  // very long localized weekdays: cap at the screen
  }
  int16_t half_h = (int16_t)(C_PILL_HALF_H * s_scale);
  uint16_t radius = (uint16_t)(C_PILL_R * s_scale);
  uint16_t border = (uint16_t)(C_PILL_BORDER * s_scale + 0.5f);

  // The pill grows from its own center out to full size and shrinks back to
  // it, rather than sliding the whole thing on and off the top of the
  // screen. Radius and border scale down with it so the shape stays in
  // proportion instead of the corners going square or the border swallowing
  // the shape as it shrinks. Below a couple of pixels there is nothing left
  // to usefully draw.
  half_w = (int16_t)(half_w * s_pill_reveal);
  half_h = (int16_t)(half_h * s_pill_reveal);
  radius = (uint16_t)(radius * s_pill_reveal);
  border = (uint16_t)(border * s_pill_reveal + 0.5f);
  if (half_w < 2 || half_h < 2) {
    return;
  }
  if (border < 1) {
    border = 1;
  }

  graphics_context_set_fill_color(ctx, s_accent);
  graphics_fill_rect(ctx, GRect(c.x - half_w, c.y - half_h, half_w * 2, half_h * 2),
                     radius, GCornersAll);
  GRect inner = GRect(c.x - half_w + border, c.y - half_h + border,
                      (half_w - border) * 2, (half_h - border) * 2);
  uint16_t inner_r = radius > border ? radius - border : 2;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, inner, inner_r, GCornersAll);

  // The content (battery gauge, date, weeks-left) only fits once the shell
  // is essentially full size — fonts cannot be scaled down to match a
  // shrinking pill, so rather than spilling out past a half-grown shell,
  // it simply isn't drawn yet on the way in and disappears at once on the
  // way out, snapping away as the shell starts shrinking rather than
  // fading or shrinking with it.
  if (s_pill_reveal < 0.92f) {
    return;
  }

  // Battery level, filling from the left inside a dark gap so that a low level
  // never merges into the border. Also drawn while animating back out.
  BatteryChargeState batt = battery_state_service_peek();
  if (s_fill > 0.0f) {
    int16_t gap = (int16_t)(C_PILL_GAP * s_scale + 0.5f);
    GRect track = GRect(inner.origin.x + gap, inner.origin.y + gap,
                        inner.size.w - 2 * gap, inner.size.h - 2 * gap);
    uint16_t track_r = inner_r > (uint16_t)gap ? inner_r - gap : 2;
    int16_t fill_w = (int16_t)(track.size.w * (batt.charge_percent / 100.0f) * s_fill + 0.5f);
    if (fill_w > 0) {
      uint16_t r = track_r;
      if (fill_w < (int16_t)(2 * r)) {
        r = fill_w / 2;  // narrow slivers: shrink the radius instead of clipping
      }
      graphics_context_set_fill_color(ctx, prv_batt_color(batt.charge_percent));
      graphics_fill_rect(ctx, GRect(track.origin.x, track.origin.y, fill_w, track.size.h),
                         r, fill_w >= track.size.w ? GCornersAll : GCornersLeft);
    }
  }

  graphics_context_set_text_color(ctx, GColorWhite);
  if (s_pill_page == PAGE_BATTERY) {
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", batt.charge_percent);
    // Bolt and percentage are centered as one group, so they cannot collide.
    int16_t shift = 0, bolt_x = 0;
    bool charging = prv_is_plugged(batt);
    if (charging) {
      GSize sz = graphics_text_layout_get_content_size(
          pct, s_pill_big_font, probe, GTextOverflowModeFill, GTextAlignmentCenter);
      int16_t group = (int16_t)((C_BOLT_W + C_BOLT_GAP) * s_scale);
      shift = group / 2;
      bolt_x = c.x - sz.w / 2 - shift;
    }
    prv_draw_text_centered(ctx, pct, s_pill_big_font, GPoint(c.x + shift, c.y),
                           PILL_BIG_DY);
    if (charging) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      gpath_move_to(s_bolt_path, GPoint(bolt_x, c.y));
      gpath_draw_filled(ctx, s_bolt_path);
    }
  } else {
    // Two-line layout shared by the date and weeks-left pages. Centred on the
    // ink these lines actually put on screen, so the room an accent needs
    // comes out of the margins the line without one does not. Reserving it
    // for the whole language instead is what would leave MONTAG sitting 5 px
    // too low all year for the sake of MÄR, or TERÇA for SÁBADO. The layout
    // moves up to 3 px on the days and in the months that carry a mark, which
    // nothing on screen is there to be compared against. Weeks-left's plain
    // ASCII label and digits never trip these checks, so it always gets the
    // plain PILL_LINE_GAP.
    //
    // Over the small line the accent needs room of its own, since there is
    // only the border above it. What hangs into the gap between the lines
    // does not: a mark over one letter or a tail under one is a small thing,
    // so it is enough to widen the gap until the clearance below it is back.
    int16_t reach = prv_is_tall(small) ? PILL_ACCENT : 0;
    int16_t into = (prv_is_deep(small) ? PILL_TAIL : 0)
                   + (prv_is_tall(big) ? PILL_ACCENT : 0);
    int16_t gap = into + PILL_MARK_CLEAR;
    if (gap < PILL_LINE_GAP) {
      gap = PILL_LINE_GAP;  // nothing hangs in, so the gap is the plain one
    }
    int16_t block = reach + PILL_SMALL_CAP + gap + PILL_BIG_CAP;
    if (block > inner.size.h) {
      gap -= block - inner.size.h;  // no locale reaches this; the border still holds
      block = inner.size.h;
    }
    int16_t cap_top = inner.origin.y + (inner.size.h - block) / 2 + reach;
    prv_draw_text_centered(ctx, small, s_pill_small_font,
                           GPoint(c.x, cap_top + PILL_SMALL_CAP / 2),
                           PILL_SMALL_DY);
    prv_draw_text_centered(ctx, big, s_pill_big_font,
                           GPoint(c.x, cap_top + PILL_SMALL_CAP + gap
                                       + PILL_BIG_CAP / 2),
                           PILL_BIG_DY);
  }
}

// ==========================================================================
// Pill animation and state
// ==========================================================================

static void prv_reveal_update(Animation *anim, const AnimationProgress progress) {
  float p = (float)progress / ANIMATION_NORMALIZED_MAX;
  p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
  s_pill_reveal = s_reveal_from + (s_reveal_to - s_reveal_from) * p;
  layer_mark_dirty(s_pill_layer);
}

// Clears the animation handle and, once a growth or shrink genuinely finishes
// rather than being interrupted by the next one, carries the pill state
// machine the rest of the way: PILL_IN -> PILL_SHOWN, or PILL_OUT ->
// PILL_HIDDEN (and only then does the layer actually stop drawing).
static void prv_reveal_stopped(Animation *anim, bool finished, void *context) {
  s_reveal_anim = NULL;  // Pebble auto-destroys the animation here
  if (!finished) {
    return;  // interrupted by a new reveal, its own stop takes over instead
  }
  if (s_pill_state == PILL_IN) {
    s_pill_state = PILL_SHOWN;
  } else if (s_pill_state == PILL_OUT) {
    s_pill_state = PILL_HIDDEN;
    layer_set_hidden(s_pill_layer, true);
    layer_mark_dirty(s_dial_layer);
  }
}

// Grows the pill from its own center out to full size, or shrinks it back.
static void prv_pill_reveal_anim(bool in) {
  if (s_reveal_anim) {
    animation_unschedule(s_reveal_anim);
    s_reveal_anim = NULL;
  }
  s_reveal_from = s_pill_reveal;
  s_reveal_to = in ? 1.0f : 0.0f;

  static const AnimationImplementation impl = { .update = prv_reveal_update };
  s_reveal_anim = animation_create();
  animation_set_implementation(s_reveal_anim, &impl);
  animation_set_duration(s_reveal_anim, PILL_REVEAL_MS);
  animation_set_curve(s_reveal_anim, in ? AnimationCurveEaseOut : AnimationCurveEaseIn);
  animation_set_handlers(s_reveal_anim, (AnimationHandlers) {
    .stopped = prv_reveal_stopped,
  }, NULL);
  animation_schedule(s_reveal_anim);
}

static void prv_fill_update(Animation *anim, const AnimationProgress progress) {
  float p = (float)progress / ANIMATION_NORMALIZED_MAX;
  p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
  s_fill = s_fill_from + (s_fill_to - s_fill_from) * p;
  layer_mark_dirty(s_pill_layer);
}

static void prv_fill_stopped(Animation *anim, bool finished, void *context) {
  s_fill_anim = NULL;  // Pebble auto-destroys the animation here
}

// Jumps the pill to a specific page. The battery level animates from
// wherever it stands, so repeated shakes do not make it jump.
static void prv_pill_set_page(uint8_t page) {
  if (s_fill_anim) {
    animation_unschedule(s_fill_anim);
    s_fill_anim = NULL;
  }
  s_pill_page = page;
  s_fill_from = s_fill;
  s_fill_to = page == PAGE_BATTERY ? 1.0f : 0.0f;

  static const AnimationImplementation impl = { .update = prv_fill_update };
  s_fill_anim = animation_create();
  animation_set_implementation(s_fill_anim, &impl);
  animation_set_duration(s_fill_anim, PILL_FILL_MS);
  animation_set_curve(s_fill_anim, AnimationCurveEaseInOut);
  animation_set_handlers(s_fill_anim, (AnimationHandlers) {
    .stopped = prv_fill_stopped,
  }, NULL);
  animation_schedule(s_fill_anim);
  layer_mark_dirty(s_pill_layer);  // the text changes at once, not with the fill
}

// Steps to the next page that is currently enabled (PAGE_DATE always is, it
// has no toggle), wrapping back around after PAGE_LIFE.
static void prv_pill_advance(void) {
  uint8_t page = s_pill_page;
  for (unsigned i = 0; i < 3; i++) {
    page = (page + 1) % 3;
    if (page == PAGE_DATE
        || (page == PAGE_BATTERY && s_pill_battery)
        || (page == PAGE_LIFE && s_pill_life)) {
      break;
    }
  }
  prv_pill_set_page(page);
}

static bool prv_charging(void) {
  return prv_is_plugged(battery_state_service_peek());
}

// True while the gauge rests: on the charger, enabled, not dismissed.
static bool prv_gauge_resting(void) {
  return s_pill_battery && !s_charge_dismissed && prv_charging();
}

static void prv_pill_cancel_timer(void) {
  if (s_pill_timer) {
    app_timer_cancel(s_pill_timer);
    s_pill_timer = NULL;
  }
}

static void prv_pill_close(void) {
  prv_pill_cancel_timer();
  if (s_pill_state == PILL_IN || s_pill_state == PILL_SHOWN) {
    s_pill_state = PILL_OUT;
    prv_pill_reveal_anim(false);
  }
}

static void prv_pill_timeout(void *data) {
  s_pill_timer = NULL;
  // A resting gauge stays up: the timeout returns it to the battery page rather
  // than closing the pill.
  if (prv_gauge_resting() && (s_pill_state == PILL_IN || s_pill_state == PILL_SHOWN)) {
    if (s_pill_page != PAGE_BATTERY) {
      prv_pill_set_page(PAGE_BATTERY);
    }
    return;
  }
  prv_pill_close();
}

// Grows the pill in on the date page. Returns false if it is already up,
// which is what lets the caller tell an opening shake from a second one.
static bool prv_pill_open(void) {
  if (s_pill_state != PILL_HIDDEN && s_pill_state != PILL_OUT) {
    return false;
  }
  animation_unschedule_all();
  if (s_pill_state == PILL_HIDDEN) {
    s_pill_reveal = 0.0f;  // start collapsed so growth begins from nothing
    layer_set_hidden(s_pill_layer, false);
    layer_mark_dirty(s_dial_layer);  // drop the minute circle at once
  }
  s_pill_page = PAGE_DATE;  // the pill always opens on the date page
  s_fill = 0.0f;             // s_fill_anim was cleared by the unschedule above
  s_pill_state = PILL_IN;
  prv_pill_reveal_anim(true);
  return true;
}

static void prv_pill_arm_timer(void) {
  if (s_pill_timer) {
    app_timer_reschedule(s_pill_timer, PILL_TIMEOUT_MS);
  } else {
    s_pill_timer = app_timer_register(PILL_TIMEOUT_MS, prv_pill_timeout, NULL);
  }
}

// ==========================================================================
// Services: shake, battery, tick
// ==========================================================================

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_shake_pill) {
    return;
  }
#if MOTION_MUTE_ENABLED
  // Tell sustained motion from a glance: on motion the pill goes down and
  // stays down until the wrist settles. A resting gauge is exempt.
  time_t now = time(NULL);
  time_t gap = now - s_last_tap;
  s_last_tap = now;
  if (s_shake_muted) {
    if (gap < SHAKE_RELEASE_S) {
      return;
    }
    s_shake_muted = false;
    s_tap_run = 1;
  } else {
    s_tap_run = (gap <= SHAKE_BURST_S) ? s_tap_run + 1 : 1;
    if (s_tap_run >= SHAKE_BURST_N && !prv_gauge_resting()) {
      s_shake_muted = true;
      prv_pill_close();
      return;
    }
  }
#endif

  if (prv_pill_open()) {
    prv_pill_arm_timer();
    return;
  }
  // The pill is already up. Watchfaces receive no button events, so the shake
  // is also the back button: with the gauge resting it walks gauge -> date ->
  // dial, and the dial stays clear until the cable changes.
  if (prv_gauge_resting() && s_pill_page != PAGE_BATTERY) {
    s_charge_dismissed = true;
    prv_pill_close();
    return;
  }
  if (s_pill_battery || s_pill_life) {
    prv_pill_advance();
  }
  prv_pill_arm_timer();
}

// Brings the pill up on the battery page. With `keep` it stays up without a
// timer, for as long as the watch is charging.
static void prv_pill_show_battery(bool keep) {
  prv_pill_open();
  if (s_pill_page != PAGE_BATTERY) {
    prv_pill_set_page(PAGE_BATTERY);
  }
  if (keep) {
    prv_pill_cancel_timer();
  } else {
    prv_pill_arm_timer();
  }
  layer_mark_dirty(s_pill_layer);  // the bolt appears or goes with the cable
}

// The service fires on every level change too, so only a change of plug state
// brings the gauge up.
static void prv_battery_handler(BatteryChargeState charge) {
  bool plugged = prv_is_plugged(charge);
  if (plugged == s_was_plugged) {
    // Level-only update: redraw a visible gauge so it tracks the level.
    if (s_pill_page == PAGE_BATTERY && s_pill_state != PILL_HIDDEN) {
      layer_mark_dirty(s_pill_layer);
    }
    return;
  }
  s_was_plugged = plugged;
  s_charge_dismissed = false;  // a change of cable starts a new session
  if (s_pill_battery) {
    prv_pill_show_battery(plugged);
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_time();
  prv_set_rotc();
  layer_mark_dirty(s_dial_layer);
}

// ==========================================================================
// Setup: paths, settings messages, lifecycle
// ==========================================================================

static void prv_build_hand_path(void) {
  // Bar through the screen center, long enough to cross the display at any
  // angle, rotated and moved as a whole by GPath.
  float hhw = C_HAND_FULL_W / 2 * s_scale;
  float hl = REF_H * s_scale;
  s_hand_points[0] = GPoint((int16_t)-hhw, (int16_t)-hl);
  s_hand_points[1] = GPoint((int16_t)hhw, (int16_t)-hl);
  s_hand_points[2] = GPoint((int16_t)hhw, (int16_t)hl);
  s_hand_points[3] = GPoint((int16_t)-hhw, (int16_t)hl);

  static GPathInfo hand_info = {4, NULL};
  hand_info.points = s_hand_points;
  s_hand_path = gpath_create(&hand_info);

  // Built empty: prv_draw_bar() writes screen coordinates into s_bar_points
  // before each fill, so this one allocation serves every mark.
  static GPathInfo bar_info = {4, NULL};
  bar_info.points = s_bar_points;
  s_bar_path = gpath_create(&bar_info);
}

// Charging bolt around its own center, so gpath_move_to() places it directly.
static void prv_build_bolt_path(void) {
  float w = C_BOLT_W / 2 * s_scale, h = C_BOLT_H / 2 * s_scale;
  s_bolt_points[0] = GPoint((int16_t)w, (int16_t)-h);
  s_bolt_points[1] = GPoint((int16_t)-w, (int16_t)(h * 0.15f));
  s_bolt_points[2] = GPoint((int16_t)(w * 0.1f), (int16_t)(h * 0.15f));
  s_bolt_points[3] = GPoint((int16_t)-w, (int16_t)h);
  s_bolt_points[4] = GPoint((int16_t)w, (int16_t)(-h * 0.15f));
  s_bolt_points[5] = GPoint((int16_t)(-w * 0.1f), (int16_t)(-h * 0.15f));

  static GPathInfo bolt_info = {6, NULL};
  bolt_info.points = s_bolt_points;
  s_bolt_path = gpath_create(&bolt_info);
}

// A tuple carries however many bytes the sender packed into it, so its length
// decides which member to read; value->int32 on a narrower one reads past it.
static int32_t prv_tuple_int(const Tuple *t) {
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);  // select values can arrive as text
  }
  switch (t->length) {
    case 0:  return 0;
    case 1:  return t->value->int8;
    case 2:  return t->value->int16;
    default: return t->value->int32;
  }
}

// Parses "YYYY-MM-DD" (Clay's date input format) by hand rather than with
// sscanf(), which drags in newlib's locale/stdio machinery and collides with
// Pebble's own libc at link time. Returns false, leaving *y/*m/*d untouched,
// on anything that isn't exactly three dash-separated digit runs.
static bool prv_parse_date(const char *s, int *y, int *m, int *d) {
  int v[3] = { 0, 0, 0 };
  int part = 0, digits = 0;
  for (const char *c = s; *c; c++) {
    if (*c == '-') {
      if (digits == 0 || ++part > 2) {
        return false;
      }
      digits = 0;
      continue;
    }
    if (*c < '0' || *c > '9') {
      return false;
    }
    v[part] = v[part] * 10 + (*c - '0');
    digits++;
  }
  if (part != 2 || digits == 0) {
    return false;
  }
  *y = v[0];
  *m = v[1];
  *d = v[2];
  return true;
}

// Applies settings from the configuration page. Every key is optional, so each
// block tests for its tuple first. Values are checked against their enum here.
static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_HAND_STYLE);
  if (t) {
    int32_t v = prv_tuple_int(t);
    s_hand_mode = (v == HAND_FULL || v == HAND_SCALE) ? v : HAND_NEEDLE;
    persist_write_int(PERSIST_KEY_HAND_MODE, s_hand_mode);
  }

  t = dict_find(iter, MESSAGE_KEY_SHOW_MINUTE);
  if (t) {
    s_show_minute = prv_tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_SHOW_MINUTE, s_show_minute);
  }

  t = dict_find(iter, MESSAGE_KEY_PILL_BATTERY);
  if (t) {
    s_pill_battery = prv_tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_PILL_BATTERY, s_pill_battery);
  }

  t = dict_find(iter, MESSAGE_KEY_MINUTE_COLON);
  if (t) {
    bool colon = prv_tuple_int(t) != 0;
    if (colon != s_minute_colon) {
      s_minute_colon = colon;
      prv_apply_minute_cut();  // a different cut means different images
    }
    persist_write_bool(PERSIST_KEY_MINUTE_COLON, s_minute_colon);
  }

  t = dict_find(iter, MESSAGE_KEY_SHAKE_PILL);
  if (t) {
    bool shake = prv_tuple_int(t) != 0;
    if (shake != s_shake_pill) {
      s_shake_pill = shake;
      // Subscribing only while the feature is on saves the tap interrupt
      // waking the CPU for a handler that would just return.
      if (s_shake_pill) {
        accel_tap_service_subscribe(prv_tap_handler);
      } else {
        accel_tap_service_unsubscribe();
      }
    }
    persist_write_bool(PERSIST_KEY_SHAKE_PILL, s_shake_pill);
  }

  t = dict_find(iter, MESSAGE_KEY_HOUR_SIZE);
  if (t) {
    int32_t v = prv_tuple_int(t);
    uint8_t size = (v >= HOUR_STD && v <= HOUR_HUGE) ? v : HOUR_STD;
    if (size != s_hour_size) {
      s_hour_size = size;
      prv_apply_hour_cut();
    }
    persist_write_int(PERSIST_KEY_HOUR_SIZE_4, s_hour_size);
  }

  t = dict_find(iter, MESSAGE_KEY_SCALE_STYLE);
  if (t) {
    int32_t v = prv_tuple_int(t);
    s_scale_style = (v == SCALE_TENTHS || v == SCALE_QUARTERS) ? v : SCALE_CLASSIC;
    persist_write_int(PERSIST_KEY_SCALE_STYLE, s_scale_style);
  }

  t = dict_find(iter, MESSAGE_KEY_ACCENT_COLOR);
  if (t) {
    int32_t hex = prv_tuple_int(t);
    persist_write_int(PERSIST_KEY_ACCENT, hex);
    s_accent = GColorFromHEX(hex);
  }

  // Clay's date input arrives as "YYYY-MM-DD". Out-of-range pieces are left
  // as whatever was already loaded rather than accepted half-parsed.
  t = dict_find(iter, MESSAGE_KEY_BIRTHDAY);
  if (t && t->type == TUPLE_CSTRING) {
    int y, m, d;
    if (prv_parse_date(t->value->cstring, &y, &m, &d)
        && y >= 1900 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
      s_birth_year = y;
      s_birth_month = m;
      s_birth_day = d;
      int32_t packed = y * 10000 + m * 100 + d;
      persist_write_int(PERSIST_KEY_BIRTHDAY, packed);
    }
  }

  t = dict_find(iter, MESSAGE_KEY_GENDER);
  if (t) {
    int32_t v = prv_tuple_int(t);
    s_gender = (v >= 0 && v <= 2) ? (uint8_t)v : 0;
    persist_write_int(PERSIST_KEY_GENDER, s_gender);
  }

  // Sent either by the Country picker in settings, or by pkjs overriding it
  // with a location-derived code when "Estimate from my location" is on.
  t = dict_find(iter, MESSAGE_KEY_COUNTRY);
  if (t && t->type == TUPLE_CSTRING) {
    const char *raw = t->value->cstring;
    if (raw[0]) {
      size_t i = 0;
      for (; i < sizeof(s_country) - 1 && raw[i]; i++) {
        s_country[i] = (char)toupper((unsigned char)raw[i]);
      }
      s_country[i] = '\0';
      persist_write_string(PERSIST_KEY_COUNTRY, s_country);
    }
  }

  t = dict_find(iter, MESSAGE_KEY_USE_LOCATION);
  if (t) {
    s_use_location = prv_tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_USE_LOCATION, s_use_location);
  }

  t = dict_find(iter, MESSAGE_KEY_PILL_LIFE);
  if (t) {
    s_pill_life = prv_tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_PILL_LIFE, s_pill_life);
  }

  if (s_dial_layer) {
    layer_mark_dirty(s_dial_layer);
  }
  if (s_pill_layer && s_pill_state != PILL_HIDDEN) {
    layer_mark_dirty(s_pill_layer);  // birthday/gender/country can change mid-view
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_w = bounds.size.w;
  s_h = bounds.size.h;
  s_scale = (float)(s_w < s_h ? s_w : s_h) / REF_H;

  window_set_background_color(window, GColorBlack);
  prv_apply_hour_cut();
  prv_apply_minute_cut();
  s_pill_small_font = fonts_load_custom_font(resource_get_handle(PILL_SMALL_FONT_RES));
  s_pill_big_font = fonts_load_custom_font(resource_get_handle(PILL_BIG_FONT_RES));
  prv_build_hand_path();
  prv_build_bolt_path();
  prv_update_time();
  prv_set_rotc();

  s_dial_layer = layer_create(bounds);
  layer_set_update_proc(s_dial_layer, prv_dial_draw);
  layer_add_child(root, s_dial_layer);

  s_pill_layer = layer_create(bounds);
  layer_set_update_proc(s_pill_layer, prv_pill_draw);
  layer_set_hidden(s_pill_layer, true);
  layer_add_child(root, s_pill_layer);

  // The firmware's charging screen rebuilds the watchface on return, so the
  // gauge is brought back if the cable is still in.
  if (s_pill_battery && prv_charging()) {
    prv_pill_show_battery(true);
  }
}

static void prv_window_unload(Window *window) {
  animation_unschedule_all();
  layer_destroy(s_pill_layer);
  layer_destroy(s_dial_layer);
  fonts_unload_custom_font(s_pill_small_font);
  fonts_unload_custom_font(s_pill_big_font);
  gpath_destroy(s_hand_path);
  gpath_destroy(s_bar_path);
  gpath_destroy(s_bolt_path);
  for (unsigned d = 0; d < 10; d++) {
    gbitmap_destroy(s_digit_bmp[d]);
  }
  for (unsigned d = 0; d < 11; d++) {
    gbitmap_destroy(s_min_bmp[d]);
  }
}

// Keeps a value read from the persist store inside its enum: a slot written by
// another version of the app would otherwise select something that does not
// exist.
static uint8_t prv_clamp(int32_t v, uint8_t max, uint8_t fallback) {
  return (v >= 0 && v <= max) ? (uint8_t)v : fallback;
}

static void prv_init(void) {
  setlocale(LC_ALL, "");  // so strftime follows the watch language
  if (persist_exists(PERSIST_KEY_HAND_MODE)) {
    s_hand_mode = prv_clamp(persist_read_int(PERSIST_KEY_HAND_MODE),
                            HAND_SCALE, HAND_NEEDLE);
  } else {
    // Migration from the "full length" boolean.
    s_hand_mode = persist_read_bool(PERSIST_KEY_HAND_FULL) ? HAND_FULL : HAND_NEEDLE;
  }
  s_show_minute = persist_exists(PERSIST_KEY_SHOW_MINUTE)
      ? persist_read_bool(PERSIST_KEY_SHOW_MINUTE) : true;
  s_pill_battery = persist_exists(PERSIST_KEY_PILL_BATTERY)
      ? persist_read_bool(PERSIST_KEY_PILL_BATTERY) : true;
  if (persist_exists(PERSIST_KEY_SCALE_STYLE)) {
    s_scale_style = prv_clamp(persist_read_int(PERSIST_KEY_SCALE_STYLE),
                              SCALE_QUARTERS, SCALE_QUARTERS);
  } else if (persist_exists(PERSIST_KEY_FINE_TICKS)) {
    // Migration from the "fine ticks" boolean. The result deliberately differs
    // from the default for a new install: a watch with the boolean set chose
    // the tenths dial, and a migration should not overrule that.
    s_scale_style = persist_read_bool(PERSIST_KEY_FINE_TICKS)
        ? SCALE_TENTHS : SCALE_CLASSIC;
  } else {
    s_scale_style = SCALE_QUARTERS;
  }
  s_shake_pill = persist_exists(PERSIST_KEY_SHAKE_PILL)
      ? persist_read_bool(PERSIST_KEY_SHAKE_PILL) : true;
  s_minute_colon = persist_exists(PERSIST_KEY_MINUTE_COLON)
      ? persist_read_bool(PERSIST_KEY_MINUTE_COLON) : false;
  if (persist_exists(PERSIST_KEY_HOUR_SIZE_4)) {
    s_hour_size = prv_clamp(persist_read_int(PERSIST_KEY_HOUR_SIZE_4),
                            HOUR_HUGE, HOUR_STD);
  } else if (persist_exists(PERSIST_KEY_HOUR_SIZE)) {
    // Migration from three steps, where 2 meant the largest cut, now 3.
    uint8_t old = (uint8_t)persist_read_int(PERSIST_KEY_HOUR_SIZE);
    s_hour_size = old == 2 ? HOUR_HUGE : old;
  } else {
    s_hour_size = HOUR_STD;
  }
  s_accent = persist_exists(PERSIST_KEY_ACCENT)
      ? GColorFromHEX(persist_read_int(PERSIST_KEY_ACCENT)) : GColorRed;

  if (persist_exists(PERSIST_KEY_BIRTHDAY)) {
    int32_t packed = persist_read_int(PERSIST_KEY_BIRTHDAY);
    s_birth_year = packed / 10000;
    s_birth_month = (packed / 100) % 100;
    s_birth_day = packed % 100;
  }  // else the 1980-01-01 compiled-in default stands
  s_gender = persist_exists(PERSIST_KEY_GENDER)
      ? prv_clamp(persist_read_int(PERSIST_KEY_GENDER), 2, 0) : 0;
  if (persist_exists(PERSIST_KEY_COUNTRY)) {
    persist_read_string(PERSIST_KEY_COUNTRY, s_country, sizeof(s_country));
  }
  s_use_location = persist_exists(PERSIST_KEY_USE_LOCATION)
      ? persist_read_bool(PERSIST_KEY_USE_LOCATION) : false;
  s_pill_life = persist_exists(PERSIST_KEY_PILL_LIFE)
      ? persist_read_bool(PERSIST_KEY_PILL_LIFE) : true;

  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(256, 64);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  // Only listen for taps while the feature that uses them is on, so a user
  // who disables the shake-to-reveal pill doesn't pay for a tap interrupt
  // that would just be filtered out in the handler.
  if (s_shake_pill) {
    accel_tap_service_subscribe(prv_tap_handler);
  }
  s_was_plugged = prv_charging();
  battery_state_service_subscribe(prv_battery_handler);
}

static void prv_deinit(void) {
  battery_state_service_unsubscribe();
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
