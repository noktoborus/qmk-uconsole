#pragma once

#include QMK_KEYBOARD_H

/* Keyboard lock (KB_LOCK): ignores keys and the trackball and turns the
 * backlight off, sending Sleep on lock and Wake on unlock. */
extern volatile bool is_locked;

/* Handles KB_LOCK and drops other keys while locked; false if consumed. */
bool process_record_lock(uint16_t keycode, keyrecord_t *record);
