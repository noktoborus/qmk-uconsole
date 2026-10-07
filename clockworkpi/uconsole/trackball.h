#ifndef TRACKBALL_H
#define TRACKBALL_H

#include "quantum.h"

/**
 * @brief Initializes the trackball hardware, GPIOs, and interrupts.
 * Configures the pins for the trackball axis inputs and enables edge-triggered events.
 */
bool pointing_device_driver_init(void);

/**
 * @brief Calculates and returns the mouse report.
 * Processes movement data, applies the velocity curve for precision, 
 * and handles the switch between mouse and wheel modes.
 * * @param mouse_report The current mouse report to be modified.
 * @return The updated report_mouse_t containing movement and wheel data.
 */
report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report);

/**
 * @brief Returns the current CPI (Counts Per Inch) setting.
 * Currently returns 0 as a placeholder.
 */
uint16_t pointing_device_driver_get_cpi(void);

/**
 * @brief Sets the CPI (Counts Per Inch) for the trackball.
 * @param cpi The desired CPI value.
 */
void pointing_device_driver_set_cpi(uint16_t cpi);

/**
 * @brief Standard QMK record processing.
 * Detects the JS_4 keypress to toggle between cursor movement and scroll wheel modes.
 */
bool process_record_kb(uint16_t keycode, keyrecord_t *record);

/* Precision mode toggle: when true, cursor movement is reduced for fine control.
 * Toggled by the TB_PREC keycode (Fn + trackball click by default).
 */
extern volatile bool precision_mode;

/* Held scroll keys (TB_SCRL, SEL_SCRL), one bit each: the ball scrolls
 * instead of moving the cursor while any is held. */
enum { SCROLL_KEY_TB_SCRL = 1 << 0, SCROLL_KEY_SEL_SCRL = 1 << 1 };
extern volatile uint8_t scroll_keys_held;

/* Trackball settings, stored in the keyboard EEPROM datablock and editable
 * from VIA (see the "Trackball" menu in via.json). Percent values scale the
 * built-in defaults, so 100 keeps the stock behaviour.
 */
typedef struct {
  uint8_t speed;        // cursor speed, % (25-200)
  uint8_t acceleration; // acceleration strength, % (0-200)
  uint8_t precision;    // cursor speed in precision mode, % (10-100)
  uint8_t glide;        // coasting after the ball stops, % (0-250, 0 = off)
  uint8_t scroll_speed; // scroll speed, % (25-250)
  uint8_t scroll_reverse; // reverse scroll direction (0/1)
  uint8_t scroll_layers; // bit N set: the ball scrolls while layer N is the highest active
  uint8_t reserved[1];
} trackball_config_t;

// Stored at the start of the keyboard EEPROM datablock
_Static_assert(sizeof(trackball_config_t) == 8, "trackball_config_t is 8 bytes");

extern trackball_config_t trackball_config;

void trackball_config_reset(void);
void trackball_config_load(void);
void trackball_config_save(void);

/* VIA set/get of the "Trackball" menu values; false if value_id isn't ours. */
bool trackball_via_command(uint8_t command_id, uint8_t value_id, uint8_t *value);
#endif /* TRACKBALL_H */