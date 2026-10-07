#include "trackball.h"
#include "backlight_fx.h"
#include "glider.h"
#include "pointing_device.h"
#include "quantum.h"
#include "rate_meter.h"
#include "eeconfig.h"
#include <math.h>
#ifdef VIA_ENABLE
#  include "via.h"
#endif

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
volatile uint8_t scroll_keys_held = 0; // set from keymap
volatile bool precision_mode = false; // toggled from keymap
extern volatile bool is_locked;

static int8_t distances[AXIS_NUM] = {0};

// Sensor steps queued by the GPIO interrupts and processed in
// pointing_device_driver_get_report(), so that the interrupts stay short and
// all (software) floating point math runs in thread context.
typedef struct {
  uint16_t time;
  uint8_t axis;
  int8_t direction;
} tb_event_t;

#define TB_EVENT_QUEUE_SIZE 32 // power of two
static tb_event_t event_queue[TB_EVENT_QUEUE_SIZE];
static volatile uint8_t event_head = 0; // written by interrupts only
static volatile uint8_t event_tail = 0; // written by the report task only
static rate_meter_t rate_meters[AXIS_NUM] = {0};
static glider_t gliders[AXIS_NUM] = {0};

// Glider units per wheel click at 100% scroll speed.
// Finer grain control for "High Res" feel
#define WHEEL_DENOM_DEFAULT 24
static int16_t wheel_denom = WHEEL_DENOM_DEFAULT;
static int16_t wheel_buffer[AXIS_NUM] = {0};

static const trackball_config_t trackball_config_defaults = {
    .speed = 100,
    .acceleration = 100,
    .precision = 50,
    .glide = 100,
    .scroll_speed = 100,
    .scroll_reverse = 0,
    .scroll_layers = 1 << 1, // Fn layer
};
trackball_config_t trackball_config;

static float glide_decay = 0.7f;

static uint8_t clamp_u8(uint8_t value, uint8_t min, uint8_t max) {
  return value < min ? min : (value > max ? max : value);
}

// Clamp values coming from EEPROM or VIA and derive cached parameters.
static void trackball_config_apply(void) {
  trackball_config_t *c = &trackball_config;
  c->speed = clamp_u8(c->speed, 25, 200);
  c->acceleration = clamp_u8(c->acceleration, 0, 200);
  c->precision = clamp_u8(c->precision, 10, 100);
  c->glide = clamp_u8(c->glide, 0, 250);
  // Glide strength scales how long the cursor coasts once the ball stops:
  // the release time, the per-report speed decay while sensor steps are
  // overdue (0.7 at 100%) and the overdue time before a full stop.
  glide_decay = (c->glide > 30) ? 1.0f - 30.0f / c->glide : 0.0f;
  c->scroll_speed = clamp_u8(c->scroll_speed, 25, 250);
  c->scroll_reverse = c->scroll_reverse ? 1 : 0;
  c->scroll_layers &= 0x0F; // DYNAMIC_KEYMAP_LAYER_COUNT layers
  wheel_denom = MAX(1, WHEEL_DENOM_DEFAULT * 100 / c->scroll_speed);
}

void trackball_config_save(void) {
  eeconfig_update_kb_datablock(&trackball_config, 0, sizeof(trackball_config));
}

void trackball_config_reset(void) {
  trackball_config = trackball_config_defaults;
  trackball_config_apply();
  trackball_config_save();
}

void trackball_config_load(void) {
  eeconfig_read_kb_datablock(&trackball_config, 0, sizeof(trackball_config));
  trackball_config_apply();
}

// Anti-rebound / Consistency Filter
// Low threshold to catch rebounds even on short movements
#define TB_LOCK_THRESHOLD 3
// Increased correction limit (2) to absorb double-tick noise bursts which are
// common with this sensor, ensuring smoothest possible glide.
#define TB_CORRECT_LIMIT 2

static int16_t consecutive_steps[AXIS_NUM] = {0};
static int8_t locked_direction[AXIS_NUM] = {0};
static int8_t correction_count[AXIS_NUM] = {0};
static uint16_t last_axis_activity[AXIS_NUM] = {0};

// Natural Acceleration Curve: High precision at low speeds, power curve at high
// speeds
static float rateToVelocityCurve(float input, float acceleration_scale) {
  float abs_input = fabsf(input);
  if (abs_input < 0.05f)
    return 0; // Lower deadzone for finer control

  float x = abs_input - 0.05f;
  float accel = ((x * x) / 60.0f) * acceleration_scale;
  float linear = x / 50.0f;

  return 0.12f + linear + accel;
}

static void trackball_move(uint8_t axis, int8_t direction, uint16_t now) {
  backlight_fx_activity(false);
  // Check for idle reset
  if (TIMER_DIFF_16(now, last_axis_activity[axis]) > 200) {
    consecutive_steps[axis] = 0;
    locked_direction[axis] = 0;
    correction_count[axis] = 0;
  }
  last_axis_activity[axis] = now;

  // Anti-rebound Filter
  bool is_reverse =
      (locked_direction[axis] != 0) && (direction != locked_direction[axis]);

  // SCENARIO 1 & 3: Axis Flipping / Rebound Filtering
  // The EVQWJN007 sensor is prone to reporting reversed direction when the ball
  // is shifted slightly (0.01mm) at the edge of a stroke or when pressure is
  // applied. This can happen on X or Y axis independently.
  //
  // STRATEGY: DROP NOISE (Do not invent data)
  // If we have established momentum (consecutive_steps >= threshold), and
  // detect a sudden reversal, we assume it is noise and DROP the packet
  // entirely.
  // - This prevents the "Glider Stop" (Zig-Zag) because we don't send the
  // reverse signal.
  // - This prevents "Jumping Around" because we don't substitute fake forward
  // motion.
  // - The cursor simply "Coasts" over the noise.

  if (is_reverse) {
    if (consecutive_steps[axis] >= TB_LOCK_THRESHOLD) {
      // Dynamic Limit:
      // Low Speed: 1 tick check (Fast response for precision)
      // High Speed: 2 tick check (Suppress mechanical bounce)
      int8_t limit = (gliders[axis].speed > 1.5f) ? 2 : 1;

      if (correction_count[axis] < limit) {
        // IGNORE this event. Treat it as if the hardware never triggered.
        correction_count[axis]++;
        return;
      } else {
        // Limit exceeded, accept the reversal as valid user intent
        locked_direction[axis] = direction;
        consecutive_steps[axis] = 1;
        correction_count[axis] = 0;
      }
    } else {
      // Not enough momentum to filter, accept immediately (allows
      // micro-adjustments)
      locked_direction[axis] = direction;
      consecutive_steps[axis] = 1;
      correction_count[axis] = 0;
    }
  } else {
    // Continuing same direction
    if (direction == locked_direction[axis]) {
      if (consecutive_steps[axis] < 32000)
        consecutive_steps[axis]++;
      correction_count[axis] = 0;
    } else {
      // First move from rest
      locked_direction[axis] = direction;
      consecutive_steps[axis] = 1;
      correction_count[axis] = 0;
    }
  }

  // Always update distances[], regardless of the mode
  distances[axis] += direction;

  // Always run glider/rate meter updates to allow momentum in both modes
  {
    rate_meter_interrupt(&rate_meters[axis], now);
    glider_set_direction(&gliders[axis], direction);

    const float rx = rate_meter_rate(&rate_meters[AXIS_X], now);
    const float ry = rate_meter_rate(&rate_meters[AXIS_Y], now);

    const float rate = sqrtf(rx * rx + ry * ry);
    const float dominant_rate = fmaxf(rx, ry);
    const float diagonal_balance =
        (dominant_rate > 0) ? (fminf(rx, ry) / dominant_rate) : 0;
    // Compensate for velocity being split across two axes.
    // Boost diagonal acceleration and reduce cardinal acceleration.
    const float acceleration_scale = (1.0f + 1.2f * diagonal_balance) *
                                     trackball_config.acceleration / 100.0f;
    float velocity = rateToVelocityCurve(rate / 4.0f, acceleration_scale) *
                     0.65f * trackball_config.speed / 100.0f;

    // Apply precision scaling if enabled
    if (precision_mode) {
      velocity *= trackball_config.precision / 100.0f;
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
    // Scale the coast time by the glide strength (0 = stop with the ball)
    gliders[axis].release = MIN(
        (uint32_t)gliders[axis].release * trackball_config.glide / 100,
        UINT16_MAX);
  }
}

// Called from the GPIO interrupts: only record the step.
static void trackball_queue_step(uint8_t axis, int8_t direction) {
  if (is_locked)
    return;
  const uint16_t now = timer_read();
  chSysLockFromISR();
  const uint8_t head = event_head;
  // Drop the step if the queue is full (the report task fell behind)
  if ((uint8_t)(head - event_tail) < TB_EVENT_QUEUE_SIZE) {
    tb_event_t *ev = &event_queue[head & (TB_EVENT_QUEUE_SIZE - 1)];
    ev->time = now;
    ev->axis = axis;
    ev->direction = direction;
    event_head = head + 1;
  }
  chSysUnlockFromISR();
}

static void trackball_left(void *arg) {
  (void)arg;
  trackball_queue_step(AXIS_X, TB_DECR);
}
static void trackball_right(void *arg) {
  (void)arg;
  trackball_queue_step(AXIS_X, TB_INCR);
}
static void trackball_up(void *arg) {
  (void)arg;
  trackball_queue_step(AXIS_Y, TB_DECR);
}
static void trackball_down(void *arg) {
  (void)arg;
  trackball_queue_step(AXIS_Y, TB_INCR);
}

// Process the steps queued since the previous report, in order.
static void trackball_process_steps(void) {
  tb_event_t ev;
  for (;;) {
    chSysLock();
    const uint8_t tail = event_tail;
    if (tail == event_head) {
      chSysUnlock();
      return;
    }
    ev = event_queue[tail & (TB_EVENT_QUEUE_SIZE - 1)];
    event_tail = tail + 1;
    chSysUnlock();

    trackball_move(ev.axis, ev.direction, ev.time);
  }
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

  // Steps happened before this report, so apply them before the decay and
  // mode-switch handling below (as when they were processed in the interrupt).
  trackball_process_steps();

  const uint16_t now = timer_read();
  const uint16_t delta = TIMER_DIFF_16(now, last_report);
  last_report = now;

  // Scroll while a scroll key is held or a scroll layer is on top
  const uint8_t layer = get_highest_layer(layer_state | default_layer_state);
  const bool scroll = scroll_keys_held ||
                      (layer < 8 && (trackball_config.scroll_layers >> layer) & 1);
  const uint8_t mode = scroll ? MODE_WHEEL : MODE_MOUSE;
  if (last_mode != mode) {
    rate_meter_expire(&rate_meters[AXIS_X]);
    rate_meter_expire(&rate_meters[AXIS_Y]);
    glider_stop(&gliders[AXIS_X]);
    glider_stop(&gliders[AXIS_Y]);
    wheel_buffer[AXIS_X] = 0;
    wheel_buffer[AXIS_Y] = 0;
    distances[AXIS_X] = 0;
    distances[AXIS_Y] = 0;
    consecutive_steps[AXIS_X] = 0;
    locked_direction[AXIS_X] = 0;
    correction_count[AXIS_X] = 0;
    consecutive_steps[AXIS_Y] = 0;
    locked_direction[AXIS_Y] = 0;
    correction_count[AXIS_Y] = 0;
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
          gliders[i].speed *= glide_decay;
          if (time_since_any >
              (uint32_t)(limit + buffer) * (200 + trackball_config.glide) / 200) {
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
    h = wheel_buffer[AXIS_X] / wheel_denom;
    v = wheel_buffer[AXIS_Y] / wheel_denom;

    // Keep remainder in buffer for next report
    wheel_buffer[AXIS_X] -= h * wheel_denom;
    wheel_buffer[AXIS_Y] -= v * wheel_denom;

    // Clear raw distances (consumed by glider logic in
    // trackball_move/glider_glide updates)
    distances[AXIS_X] = 0;
    distances[AXIS_Y] = 0;
    break;
  }

  mouse_report.x = x;
  mouse_report.y = y;
  mouse_report.h = h;
  // Inverted by default for natural scroll direction
  mouse_report.v = trackball_config.scroll_reverse ? v : -v;
  return mouse_report;
}

uint16_t pointing_device_driver_get_cpi(void) { return 0; }
void pointing_device_driver_set_cpi(uint16_t cpi) { (void)cpi; }

#ifdef VIA_ENABLE
// VIA "Trackball" menu (via.json): value ids on the custom channel
enum {
  id_tb_speed = 1,
  id_tb_acceleration,
  id_tb_precision,
  id_tb_glide,
  id_tb_scroll_speed,
  id_tb_scroll_reverse,
  id_tb_layer0_mode, // .. id_tb_layer0_mode + 3: 0 = cursor, 1 = scroll
};

static uint8_t *trackball_config_value(uint8_t value_id) {
  switch (value_id) {
  case id_tb_speed:
    return &trackball_config.speed;
  case id_tb_acceleration:
    return &trackball_config.acceleration;
  case id_tb_precision:
    return &trackball_config.precision;
  case id_tb_glide:
    return &trackball_config.glide;
  case id_tb_scroll_speed:
    return &trackball_config.scroll_speed;
  case id_tb_scroll_reverse:
    return &trackball_config.scroll_reverse;
  default:
    return NULL;
  }
}

bool trackball_via_command(uint8_t command_id, uint8_t value_id, uint8_t *value) {
  // Per-layer ball mode: one bit of scroll_layers each
  if (value_id >= id_tb_layer0_mode && value_id < id_tb_layer0_mode + 4) {
    const uint8_t bit = 1 << (value_id - id_tb_layer0_mode);
    switch (command_id) {
    case id_custom_set_value:
      trackball_config.scroll_layers =
          *value ? (trackball_config.scroll_layers | bit) : (trackball_config.scroll_layers & ~bit);
      break;
    case id_custom_get_value:
      *value = (trackball_config.scroll_layers & bit) ? 1 : 0;
      break;
    }
    return true;
  }

  uint8_t *field = trackball_config_value(value_id);
  if (field == NULL)
    return false;

  switch (command_id) {
  case id_custom_set_value:
    *field = *value;
    trackball_config_apply();
    break;
  case id_custom_get_value:
    *value = *field;
    break;
  }
  return true;
}
#endif // VIA_ENABLE

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
  if (record->event.pressed) {
    backlight_fx_activity(true);
  }
  return process_record_user(keycode, record);
}
