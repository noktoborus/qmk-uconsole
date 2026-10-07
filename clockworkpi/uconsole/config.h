#pragma once

#include_next <config.h>

#define BACKLIGHT_LEVELS 10

// QMK PWM backlight driver on A8 = TIM1_CH1. 6 MHz counter (72 MHz / 12) with
// a 1200 tick period gives 5 kHz PWM, like the former custom driver: no
// visible flicker (the driver default is 256 Hz) and fine steps for fades.
#define BACKLIGHT_PWM_DRIVER PWMD1
#define BACKLIGHT_PWM_CHANNEL 1
#define BACKLIGHT_PAL_MODE PAL_MODE_STM32_ALTERNATE_PUSHPULL
#define BACKLIGHT_PWM_COUNTER_FREQUENCY 6000000
#define BACKLIGHT_PWM_PERIOD 1200

// VIA: four dynamic layers (LY0-LY3) stored in EEPROM
#define DYNAMIC_KEYMAP_LAYER_COUNT 4

// Trackball (trackball_config_t) and backlight effect (backlight_fx_config_t)
// settings stored in EEPROM, editable from VIA
#define EECONFIG_KB_DATA_SIZE 16
// Bump when the EEPROM layout or the default keymap changes: a mismatch
// resets the whole EEPROM, so VIA reloads the keymap compiled into the firmware
#define EECONFIG_KB_DATA_VERSION 0x55430007

// Halve QMK keyboard mousekey speed (default MOVE_DELTA is 8)
#define MOUSEKEY_MOVE_DELTA 8
#define MOUSEKEY_MAX_SPEED 2
#define MOUSEKEY_TIME_TO_MAX 40

