#include "gamepad.h"

bool process_record_gamepad(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
  case JS_LEFT:
    joystick_set_axis(1, record->event.pressed ? -127 : 0);
    return false;
  case JS_RGHT:
    joystick_set_axis(1, record->event.pressed ? 127 : 0);
    return false;
  case JS_UP:
    joystick_set_axis(0, record->event.pressed ? -127 : 0);
    return false;
  case JS_DOWN:
    joystick_set_axis(0, record->event.pressed ? 127 : 0);
    return false;
  case JS_A ... JS_R:
    if (record->event.pressed) {
      register_joystick_button(keycode - JS_A);
    } else {
      unregister_joystick_button(keycode - JS_A);
    }
    return false;
  default:
    return true;
  }
}
