#include "trackball.h"
#include "glider.h"
#include "pointing_device.h"
#include "quantum.h"
#include "rate_meter.h"
#include <math.h>

#define TB_LEFT PAL_LINE(GPIOC, 11U)
#define TB_RIGHT PAL_LINE(GPIOC, 9U)
#define TB_UP PAL_LINE(GPIOC, 8U)
#define TB_DOWN PAL_LINE(GPIOC, 10U)

#define TB_DECR -1
#define TB_INCR 1

enum { AXIS_X = 0, AXIS_Y, AXIS_NUM };
enum { MODE_WHEEL, MODE_MOUSE };

static uint8_t last_mode = MODE_MOUSE;
static uint16_t last_report = 0;
volatile bool select_button_pressed = false; // toggled from keymap
volatile bool select_button_scrolled =
    false; // set when trackball moves while select is pressed
volatile bool precision_mode = false; // toggled from keymap

extern volatile bool is_locked;

static int8_t distances[AXIS_NUM] = {0};
static rate_meter_t rate_meters[AXIS_NUM] = {0};
static glider_t gliders[AXIS_NUM] = {0};

static const int16_t WHEEL_DENOM =
    24; // Finer grain control for "High Res" feel
static int16_t wheel_buffer[AXIS_NUM] = {0};



// Natural Acceleration Curve: High precision at low speeds, power curve at high
// speeds
static float rateToVelocityCurve(float input, float acceleration_scale) {
  float abs_input = fabsf(input);

  // Smoothly ramp up the base offset from 0 to 0.12 using a rational function,
  // avoiding the hard deadzone and sudden jump of the previous curve.
  // This provides natural, immediate response to slow, fine movements.
  float base = 0.12f * (abs_input / (abs_input + 0.04f));

  float accel = ((abs_input * abs_input) / 60.0f) * acceleration_scale;
  float linear = abs_input / 50.0f;

  return base + linear + accel;
}

static void trackball_move(uint8_t axis, int8_t direction) {
  if (is_locked)
    return;

  // Always update distances[], regardless of the mode
  distances[axis] += direction;

  // Always run glider/rate meter updates to allow momentum in both modes
  {
    rate_meter_interrupt(&rate_meters[axis]);
    glider_set_direction(&gliders[axis], direction);

    const float rx = rate_meter_rate(&rate_meters[AXIS_X]);
    const float ry = rate_meter_rate(&rate_meters[AXIS_Y]);

    const float rate = sqrtf(rx * rx + ry * ry);
    const float dominant_rate = fmaxf(rx, ry);
    const float diagonal_balance =
        (dominant_rate > 0) ? (fminf(rx, ry) / dominant_rate) : 0;
    // Compensate for velocity being split across two axes.
    // Boost diagonal acceleration and reduce cardinal acceleration.
    const float acceleration_scale = 1.0f + 1.2f * diagonal_balance;
    float velocity =
        rateToVelocityCurve(rate / 4.0f, acceleration_scale) * 0.65f;

    // Apply precision scaling if enabled
    if (precision_mode) {
      velocity *= 0.5f; // 50% speed for high precision
    }

    const float ratio = (rate > 0) ? (velocity / rate) : 0;

    const float vx = rx * ratio;
    const float vy = ry * ratio;

    uint16_t sustain_x = rate_meter_delta(&rate_meters[AXIS_X]);
    if (sustain_x < 5)
      sustain_x = 5;
    if (sustain_x > 20)
      sustain_x = 20;

    uint16_t sustain_y = rate_meter_delta(&rate_meters[AXIS_Y]);
    if (sustain_y < 5)
      sustain_y = 5;
    if (sustain_y > 20)
      sustain_y = 20;

    if (axis == AXIS_X) {
      glider_update(&gliders[AXIS_X], vx, sustain_x);
      glider_update_speed(&gliders[AXIS_Y], vy);
    } else {
      glider_update_speed(&gliders[AXIS_X], vx);
      glider_update(&gliders[AXIS_Y], vy, sustain_y);
    }
  }

  if (select_button_pressed) {
    select_button_scrolled = true;
  }
}

static void trackball_left(void *arg) {
  (void)arg;
  trackball_move(AXIS_X, TB_DECR);
}
static void trackball_right(void *arg) {
  (void)arg;
  trackball_move(AXIS_X, TB_INCR);
}
static void trackball_up(void *arg) {
  (void)arg;
  trackball_move(AXIS_Y, TB_DECR);
}
static void trackball_down(void *arg) {
  (void)arg;
  trackball_move(AXIS_Y, TB_INCR);
}

bool pointing_device_driver_init(void) {
  palSetLineMode(TB_LEFT, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(TB_RIGHT, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(TB_UP, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(TB_DOWN, PAL_MODE_INPUT_PULLUP);

  palEnableLineEvent(TB_LEFT, PAL_EVENT_MODE_BOTH_EDGES);
  palEnableLineEvent(TB_RIGHT, PAL_EVENT_MODE_BOTH_EDGES);
  palEnableLineEvent(TB_UP, PAL_EVENT_MODE_BOTH_EDGES);
  palEnableLineEvent(TB_DOWN, PAL_EVENT_MODE_BOTH_EDGES);

  palSetLineCallback(TB_LEFT, trackball_left, NULL);
  palSetLineCallback(TB_RIGHT, trackball_right, NULL);
  palSetLineCallback(TB_UP, trackball_up, NULL);
  palSetLineCallback(TB_DOWN, trackball_down, NULL);
  return true;
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
  int8_t x = 0, y = 0, h = 0, v = 0;
  chSysLock();

  const uint16_t now = timer_read();
  uint16_t delta = TIMER_DIFF_16(now, last_report);
  last_report = now;

  // Prevent massive cursor jumps when waking from an idle state.
  // If the trackball wasn't polled for a long time, delta will be huge.
  // Applying a huge delta to the instantaneous wake-up speed causes overshoot.
  if (delta > 50) {
      delta = 50;
  }

  const uint8_t mode = select_button_pressed ? MODE_WHEEL : MODE_MOUSE;
  if (last_mode != mode) {
    rate_meter_expire(&rate_meters[AXIS_X]);
    rate_meter_expire(&rate_meters[AXIS_Y]);
    glider_stop(&gliders[AXIS_X]);
    glider_stop(&gliders[AXIS_Y]);
    wheel_buffer[AXIS_X] = 0;
    wheel_buffer[AXIS_Y] = 0;
    distances[AXIS_X] = 0;
    distances[AXIS_Y] = 0;
  } else {
    rate_meter_tick(&rate_meters[AXIS_X], delta);
    rate_meter_tick(&rate_meters[AXIS_Y], delta);

    // Active decay/cutoff logic for gliders when expected interrupts are
    // overdue.
    uint16_t time_since_X =
        TIMER_DIFF_16(now, rate_meters[AXIS_X].last_time_millis);
    uint16_t time_since_Y =
        TIMER_DIFF_16(now, rate_meters[AXIS_Y].last_time_millis);
    uint16_t time_since_any =
        (time_since_X < time_since_Y) ? time_since_X : time_since_Y;

    for (int i = 0; i < AXIS_NUM; i++) {
      if (gliders[i].speed != 0) {
        uint16_t limit = rate_meters[i].average_delta;
        uint16_t buffer = (limit < 20) ? 5 : (limit / 4);
        if (time_since_any > limit + buffer) {
          gliders[i].sustain = 0;
          gliders[i].speed *= 0.7f;
          if (time_since_any > (limit + buffer) * 3 / 2) {
            glider_stop(&gliders[i]);
          }
        }
      }
    }
  }
  last_mode = mode;

  switch (mode) {
  case MODE_MOUSE:
    x = glider_glide(&gliders[AXIS_X], (uint8_t)delta);
    y = glider_glide(&gliders[AXIS_Y], (uint8_t)delta);
    distances[AXIS_X] = 0;
    distances[AXIS_Y] = 0;
    break;
  case MODE_WHEEL:
    // Use glider for smoothed momentum scrolling
    // Accumulate smoothed movement into wheel buffer
    // Note: We use the same gliders as mouse mode for consistent feel
    wheel_buffer[AXIS_X] += glider_glide(&gliders[AXIS_X], (uint8_t)delta);
    wheel_buffer[AXIS_Y] += glider_glide(&gliders[AXIS_Y], (uint8_t)delta);

    // Calculate scroll amount from accumulated buffer
    h = wheel_buffer[AXIS_X] / WHEEL_DENOM;
    v = wheel_buffer[AXIS_Y] / WHEEL_DENOM;

    // Keep remainder in buffer for next report
    wheel_buffer[AXIS_X] -= h * WHEEL_DENOM;
    wheel_buffer[AXIS_Y] -= v * WHEEL_DENOM;

    // Clear raw distances (consumed by glider logic in
    // trackball_move/glider_glide updates)
    distances[AXIS_X] = 0;
    distances[AXIS_Y] = 0;
    break;
  }
  chSysUnlock();

  mouse_report.x = x;
  mouse_report.y = y;
  mouse_report.h = h;
  mouse_report.v = -v; // Inverted for natural scroll direction
  return mouse_report;
}

uint16_t pointing_device_driver_get_cpi(void) { return 0; }
void pointing_device_driver_set_cpi(uint16_t cpi) { (void)cpi; }

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
  return process_record_user(keycode, record);
}
