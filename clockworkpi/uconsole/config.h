#pragma once

#include_next <config.h>

#define BACKLIGHT_LEVELS 10

// VIA: four dynamic layers (LY0-LY3) stored in EEPROM
#define DYNAMIC_KEYMAP_LAYER_COUNT 4

// Trackball settings (trackball_config_t) stored in EEPROM, editable from VIA
#define EECONFIG_KB_DATA_SIZE 8
// Bump when the EEPROM layout or the default keymap changes: a mismatch
// resets the whole EEPROM, so VIA reloads the keymap compiled into the firmware
#define EECONFIG_KB_DATA_VERSION 0x55430003

// Halve QMK keyboard mousekey speed (default MOVE_DELTA is 8)
#define MOUSEKEY_MOVE_DELTA 8
#define MOUSEKEY_MAX_SPEED 2
#define MOUSEKEY_TIME_TO_MAX 40

