#pragma once

#include "quantum.h"

/* Backlight effects on top of the QMK PWM backlight driver: idle dimming and
 * a brightness boost on key presses, both computed here on one lightness
 * value so that switching between them is smooth. The driver still
 * owns the level (Fn+Space, VIA brightness); these effects drive the PWM.
 * Settings are stored in the keyboard EEPROM datablock right after the
 * trackball settings and are editable from VIA ("Backlight" menu).
 */
typedef struct {
  uint8_t idle_timeout;    // seconds without activity before dimming, 0 = off
  uint8_t dim_fade;        // fade-out time when going idle, 0.1 s (1-50)
  uint8_t wake_fade;       // fade-in time on activity, 0.1 s (1-50)
  uint8_t boost;           // key press brightness boost, % of headroom (0-100)
  uint8_t boost_fade;      // boost fade-out time, 0.1 s (1-50)
  uint8_t reserved[3];
} backlight_fx_config_t;

#define BACKLIGHT_FX_CONFIG_OFFSET 8

void backlight_fx_config_reset(void);
void backlight_fx_config_load(void);

/* Call on user activity; key presses also trigger the boost. */
void backlight_fx_activity(bool key_press);

/* Drives the PWM; call from housekeeping_task_kb(). */
void backlight_fx_task(void);

/* VIA custom values of the "Backlight" menu; false if value_id isn't ours. */
bool backlight_fx_via_command(uint8_t command_id, uint8_t value_id, uint8_t *value);
