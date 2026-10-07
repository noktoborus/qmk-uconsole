#pragma once

#include QMK_KEYBOARD_H

/* Gamepad keycodes: JS_LEFT..JS_DOWN drive the joystick axes, JS_A..JS_R
 * press joystick buttons 0-7; false if consumed. */
bool process_record_gamepad(uint16_t keycode, keyrecord_t *record);
