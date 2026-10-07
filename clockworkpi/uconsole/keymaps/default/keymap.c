// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "trackball.h"

enum {
  LY0 = 0,
  LY1,
  LY2,
  LY3
};

enum {
  // Keyboard-range keycodes so VIA can show and assign them (customKeycodes in via.json)
  JS_LEFT = QK_KB_0,
  JS_RGHT,
  JS_UP,
  JS_DOWN,
  KB_LOCK,
  TB_PREC,  // Toggle trackball precision mode
  // Gamepad buttons (QMK JS_0..JS_5), so VIA can show and assign them
  JS_A,
  JS_B,
  JS_X,
  JS_Y,
  JS_SEL,
  JS_STA,
  JS_L,     // Gamepad shoulder buttons (QMK JS_6, JS_7)
  JS_R,
  TB_SCRL,  // Trackball scrolls while held
  SEL_SCRL  // Select key; the trackball also scrolls while it is held
};

const key_override_t vol_key_override =
  ko_make_basic(MOD_MASK_SHIFT, KC_VOLD, KC_VOLU);

const key_override_t *key_overrides[] = {&vol_key_override};

combo_t key_combos[] = {};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
     * Layer 0: Default
     *
     *   (Up)             ( L ) ( R )          ( Y ) ( X )
     * (Lt)  (Rt)                               ( B ) ( A )
     *   (Dn)                                     (o - middle click - trackball.c)
     *
     * (Esc)(Sel)(Sta)     (Vol)( [ )( ] )( / )( - )( = )( \ )
     * ( ~ )( 1 )( 2 )( 3 )( 4 )( 5 )( 6 )( 7 )( 8 )( 9 )( 0 )(Bsp)
     * (Tab)( Q )( W )( E )( R )( T )( Y )( U )( I )( O )( P )
     * ( ' )( A )( S )( D )( F )( G )( H )( J )( K )( L )( ; )(Ent)
     * (Sft)( Z )( X )( C )( V )( B )( N )( M )( , )( . )(Sft)
     *  (Fn)(Ctl)(Alt)(       Space      )(Alt)(Ctl)(Fn)
     *
     */
    [LY0] = LAYOUT(
        /* JS_0 A -> WWW Forward
         * JS_1 B -> WWW Backward
         * JS_2 Y -> Menu
         * JS_3 X -> SysRq */   // formerly JS_0,    JS_1,    JS_2,    JS_3,
        KC_UP,   KC_DOWN, KC_LEFT, KC_RGHT, KC_WFWD, KC_WBAK, KC_SYRQ, KC_MENU,
        KC_LSFT, KC_RSFT, KC_LCTL, KC_RCTL, KC_LALT, MS_BTN1, KC_RALT, MS_BTN2,
        MS_BTN3, KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,

        /* JS_4 Select -> (Keyboard) Select
         * JS_5 Start  -> Left GUI */
        SEL_SCRL,  KC_LGUI, KC_VOLD, KC_GRV,  KC_LBRC, KC_RBRC, KC_MINS, KC_EQL,
        KC_1,      KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,
        KC_9,      KC_0,    KC_ESC,  KC_TAB,  KC_NO,   KC_NO,   KC_NO,   KC_NO,
        KC_Q,      KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,
        KC_O,      KC_P,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,
        KC_J,      KC_K,    KC_L,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,
        KC_N,      KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_BSLS, KC_SCLN, KC_QUOT,
        KC_BSPC,   KC_ENT,  MO(LY1), MO(LY1), KC_SPC,  KC_NO,   KC_NO,   KC_NO
    ),

    /*
     * Layer 1: Fn Keys
     *
     *   (PgU)            ( L ) ( R )          (   ) (   )
     * (Hom) (End)                               (   ) (   )
     *   (PgD)                                    (o - precision mode)
     *
     * (Lck)(Prt)(Pau)     (Mut)(   )(   )(   )(F11)(F12)(   )
     * (   )(F1 )(F2 )(F3 )(F4 )(F5 )(F6 )(F7 )(F8 )(F9 )(F10)(Del)
     * (Cap)(   )(   )(   )(   )(   )(   )(PgU)(Ins)(   )(   )
     * (   )(   )(   )(   )(   )(Tg2)(Hom)(End)(PgD)(   )(   )(   )
     * (Hom)(PgD)(   )(   )(   )(   )(   )(   )
     * (Rst)(   )(Cmd)(      BlStp       )(Cmd)(   )(Rst)
     * Rst = Fn+Fn: reboot into the bootloader (2-3 s window for dfu-util)
     */

    [LY1] = LAYOUT(
        KC_PGUP, KC_PGDN, KC_HOME, KC_END,  _______, _______, _______, _______,
        _______, _______, _______, _______, KC_LGUI, _______, KC_RGUI, _______,
        TB_PREC, _______, _______, _______, _______, _______, _______, _______,

        KC_PSCR, KC_PAUS, KC_MUTE, _______, _______, _______, KC_F11,  KC_F12,
        KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,
        KC_F9,   KC_F10,  KB_LOCK, KC_CAPS, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, KC_PGUP, KC_INS,
        _______, _______, _______, _______, _______, _______, TG(LY2), KC_HOME,
        KC_END,  KC_PGDN, _______, _______, KC_CUT, KC_COPY,  KC_PSTE, _______,
        _______, _______, KC_BRID, KC_BRIU, _______, _______, _______, _______,
        KC_DEL,  _______, QK_BOOT, QK_BOOT, BL_STEP, _______, _______, _______
    ),

    /*
     * Layer 2: Gamepad (Fn + G toggles it on and off: G is transparent here,
     * so with Fn held it reaches TG(LY2) on the Fn layer)
     *
     *   (JS↑)            ( L )                ( Y ) ( X )
     * (JS←) (JS→)           ( R )                ( B ) ( A )
     *   (JS↓)                                (o)
     *
     * (   )(Sel)(Sta)     (   )(   )(   )(   )(   )(   )(   )
     * D-pad: joystick axes; A B X Y Sel Sta L R: joystick buttons 0-7
     */
    [LY2] = LAYOUT(
        JS_LEFT, JS_RGHT, JS_UP,   JS_DOWN, JS_A,    JS_B,    JS_X,    JS_Y,
        _______, _______, _______, _______, _______, JS_L,    _______, JS_R,
        _______, _______, _______, _______, _______, _______, _______, _______,

        JS_SEL,  JS_STA,  _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______
    ),

    /*
     * Layer 3: empty, free to configure from VIA
     */
    [LY3] = LAYOUT(
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,

        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______
    ),
};

volatile bool is_locked = false;

// Extern from trackball.c to control scroll mode
extern volatile bool precision_mode;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  if (is_locked && keycode != KB_LOCK && keycode != MO(LY1)) {
    return false;
  }

  switch (keycode) {
  case KB_LOCK: {
    uint16_t code = is_locked ? KC_WAKE : KC_SLEP;
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
    case TB_SCRL:
    case SEL_SCRL: {
      const uint8_t bit = (keycode == TB_SCRL) ? SCROLL_KEY_TB_SCRL : SCROLL_KEY_SEL_SCRL;
      if (record->event.pressed) {
        scroll_keys_held |= bit;
      } else {
        scroll_keys_held &= ~bit;
      }
      // SEL_SCRL also sends the Select key
      if (keycode == SEL_SCRL) {
        if (record->event.pressed) {
          register_code(KC_SELECT);
        } else {
          unregister_code(KC_SELECT);
        }
      }
      return false;
    }
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
    case TB_PREC:
      if (record->event.pressed) {
        precision_mode = !precision_mode;
      }
      return false;
    default:
      return true;
  }
}
