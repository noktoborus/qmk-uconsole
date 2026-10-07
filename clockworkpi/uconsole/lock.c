#include "lock.h"

volatile bool is_locked = false;

bool process_record_lock(uint16_t keycode, keyrecord_t *record) {
  // While locked only KB_LOCK (on the Fn layer) and Fn itself work
  if (is_locked && keycode != KB_LOCK && keycode != MO(1)) {
    return false;
  }
  if (keycode != KB_LOCK) {
    return true;
  }

  const uint16_t code = is_locked ? KC_WAKE : KC_SLEP;
  if (record->event.pressed) {
    is_locked = !is_locked;
    if (is_locked) {
      backlight_disable();
    } else {
      backlight_enable();
    }
    register_code(code);
  } else {
    unregister_code(code);
  }
  return false;
}
