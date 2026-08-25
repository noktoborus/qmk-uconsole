#include "raw_hid.h"
#include "backlight.h"
#include "eeconfig.h"
#include "host.h"
#include "quantum.h"
#include "usb_descriptor.h"
#include <string.h>

extern volatile bool is_locked;
extern volatile bool precision_mode;

void raw_hid_receive(uint8_t *data, uint8_t length) {
  uint8_t response[RAW_EPSIZE] = {0};

  response[0] = data[0]; // Echo command
  switch (data[0]) {
  // Lock block
  case 0x10:
    response[1] = is_locked;
    host_raw_hid_send(response, RAW_EPSIZE);
    break;
  case 0x12:
    is_locked = true;
    backlight_disable();
    break;
  case 0x13:
    is_locked = false;
    backlight_enable();
    break;

  // Trackball precision mode
  case 0x90:
    response[1] = precision_mode;
    host_raw_hid_send(response, RAW_EPSIZE);
    break;
  case 0x91:
    precision_mode = !precision_mode;
    break;
  case 0x92:
    precision_mode = true;
    break;
  case 0x93:
    precision_mode = false;
    break;

  // Backlight block
  case 0xb0:
    response[1] = is_backlight_enabled();
    response[2] = get_backlight_level();
    host_raw_hid_send(response, RAW_EPSIZE);
    break;
  case 0xb1:
    if (is_backlight_enabled()) {
      backlight_disable();
    } else {
      backlight_enable();
    }
    break;
  case 0xb2:
    backlight_disable();
    break;
  case 0xb3:
    backlight_enable();
    break;
  case 0xb4:
    backlight_level(data[1]);
    break;
  case 0xfa:
    break;
  }
  if (memcmp(data, "\xfa\xfa\xfa", 3) == 0) {
#ifdef NO_RESET
    eeconfig_init();
#else
    eeconfig_disable();
    soft_reset_keyboard();
#endif
  }
}
