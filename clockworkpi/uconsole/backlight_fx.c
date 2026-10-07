#include "backlight_fx.h"
#include "backlight.h"
#include "eeconfig.h"
#include <hal.h>
#ifdef VIA_ENABLE
#  include "via.h"
#endif

_Static_assert(BACKLIGHT_FX_CONFIG_OFFSET + sizeof(backlight_fx_config_t) <= EECONFIG_KB_DATA_SIZE,
               "backlight_fx_config_t doesn't fit EECONFIG_KB_DATA_SIZE");

#define FX_TICK_MS 4
#define LIGHTNESS_MAX 0xFFFFu
// The boost stays at its peak this long after the last key press
#define BOOST_HOLD_MS 100

static const backlight_fx_config_t defaults = {
    .idle_timeout = 5,
    .dim_fade = 10,
    .wake_fade = 2,
    .boost = 0,
    .boost_fade = 3,
};
static backlight_fx_config_t config;

static uint32_t last_activity = 0;
static uint32_t last_press = 0;
static uint16_t last_tick = 0;
static uint16_t lightness = 0; // what the PWM currently shows
static bool following = true;  // lightness tracks the level exactly

static uint8_t clamp_u8(uint8_t value, uint8_t min, uint8_t max) {
  return value < min ? min : (value > max ? max : value);
}

static void config_apply(void) {
  config.dim_fade = clamp_u8(config.dim_fade, 1, 50);
  config.wake_fade = clamp_u8(config.wake_fade, 1, 50);
  config.boost = clamp_u8(config.boost, 0, 100);
  config.boost_fade = clamp_u8(config.boost_fade, 1, 50);
}

static void config_save(void) {
  eeconfig_update_kb_datablock(&config, BACKLIGHT_FX_CONFIG_OFFSET, sizeof(config));
}

void backlight_fx_config_reset(void) {
  config = defaults;
  config_apply();
  config_save();
}

void backlight_fx_config_load(void) {
  eeconfig_read_kb_datablock(&config, BACKLIGHT_FX_CONFIG_OFFSET, sizeof(config));
  config_apply();
}

void backlight_fx_activity(bool key_press) {
  last_activity = timer_read32();
  if (key_press) {
    last_press = last_activity;
  }
}

// Same perceptual curve as the QMK PWM driver, so the levels look the same
// (see platforms/chibios/drivers/backlight_pwm.c)
static uint16_t cie_lightness(uint16_t v) {
  if (v <= 5243)
    return v / 9;
  uint32_t y = (((uint32_t)v + 10486) << 8) / (10486 + 0xFFFFUL);
  y = y * y * y >> 8;
  return y > 0xFFFFUL ? 0xFFFFU : (uint16_t)y;
}

static void pwm_write(uint16_t value) {
  if (value == 0) {
    pwmDisableChannel(&BACKLIGHT_PWM_DRIVER, BACKLIGHT_PWM_CHANNEL - 1);
  } else {
    pwmEnableChannel(&BACKLIGHT_PWM_DRIVER, BACKLIGHT_PWM_CHANNEL - 1,
                     PWM_FRACTION_TO_WIDTH(&BACKLIGHT_PWM_DRIVER, 0xFFFF, cie_lightness(value)));
  }
}

// Lightness of the current backlight level
static uint16_t level_lightness(void) {
  if (!is_backlight_enabled())
    return 0;
  return LIGHTNESS_MAX * get_backlight_level() / BACKLIGHT_LEVELS;
}

void backlight_fx_task(void) {
  const uint16_t now = timer_read();
  const uint16_t dt = TIMER_DIFF_16(now, last_tick);
  if (dt < FX_TICK_MS)
    return;
  last_tick = now;

  const uint32_t now32 = timer_read32();
  const uint16_t level = level_lightness();

  const bool idle = config.idle_timeout &&
                    TIMER_DIFF_32(now32, last_activity) >= config.idle_timeout * 1000UL;
  // No boost while the backlight is off (level 0, disabled or keyboard locked)
  const bool boost = config.boost && level > 0 && !idle &&
                     TIMER_DIFF_32(now32, last_press) < config.wake_fade * 100UL + BOOST_HOLD_MS;

  uint16_t target = level;
  if (idle) {
    target = 0;
  } else if (boost) {
    target = level + (uint32_t)(LIGHTNESS_MAX - level) * config.boost / 100;
  }

  if (idle || boost) {
    following = false;
  }

  if (following) {
    lightness = target;
  } else {
    // Fade towards the target; fade times are for the full lightness range
    const uint8_t fade = (lightness < target) ? config.wake_fade
                         : idle               ? config.dim_fade
                                              : config.boost_fade;
    uint32_t step = (uint32_t)LIGHTNESS_MAX * dt / (fade * 100UL);
    if (step == 0)
      step = 1;
    if (lightness < target) {
      lightness = (target - lightness > step) ? lightness + step : target;
    } else {
      lightness = (lightness - target > step) ? lightness - step : target;
    }
    // Caught up with the level again: follow it exactly
    if (!idle && !boost && lightness == target) {
      following = true;
    }
  }

  // Written every tick: also overrides the driver after it sets a level
  pwm_write(lightness);
}

#ifdef VIA_ENABLE
// VIA "Backlight" menu (via.json): value ids on the custom channel
enum {
  id_bl_idle_timeout = 16,
  id_bl_dim_fade,
  id_bl_wake_fade,
  id_bl_boost = 21,
  id_bl_boost_fade,
};

static uint8_t *config_value(uint8_t value_id) {
  switch (value_id) {
  case id_bl_idle_timeout:
    return &config.idle_timeout;
  case id_bl_dim_fade:
    return &config.dim_fade;
  case id_bl_wake_fade:
    return &config.wake_fade;
  case id_bl_boost:
    return &config.boost;
  case id_bl_boost_fade:
    return &config.boost_fade;
  default:
    return NULL;
  }
}

bool backlight_fx_via_command(uint8_t command_id, uint8_t value_id, uint8_t *value) {
  uint8_t *field = config_value(value_id);
  if (field == NULL)
    return false;
  switch (command_id) {
  case id_custom_set_value:
    *field = *value;
    config_apply();
    break;
  case id_custom_get_value:
    *value = *field;
    break;
  case id_custom_save:
    config_save();
    break;
  }
  return true;
}
#endif // VIA_ENABLE
