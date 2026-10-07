#include "quantum.h"
#include "trackball.h"
#include "backlight_fx.h"
#include "gamepad.h"
#include "lock.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif

// Helper to safely clear the backup register
void clear_bootloader_flag(void) {
    // 1. Enable power and backup interface clocks
    RCC->APB1ENR |= (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN);

    // 2. Enable access to the backup registers
    PWR->CR |= PWR_CR_DBP;

    // 3. Clear the magic value in BKP_DR10
    BKP->DR10 = 0;

    // 4. Disable backup domain access again
    PWR->CR &= ~PWR_CR_DBP;
}

void keyboard_pre_init_kb(void) {
    clear_bootloader_flag();
    keyboard_pre_init_user();
}

// Keyboard EEPROM datablock: trackball settings, then backlight effects
void eeconfig_init_kb_datablock(void) {
    trackball_config_reset();
    backlight_fx_config_reset();
}

void keyboard_post_init_kb(void) {
    if (!eeconfig_is_kb_datablock_valid()) {
        // EEPROM written by an older firmware (different layout or keymap):
        // reset everything, including the VIA keymap. This also writes the
        // defaults through eeconfig_init_kb_datablock().
        eeconfig_init();
    } else {
        trackball_config_load();
        backlight_fx_config_load();
    }
    keyboard_post_init_user();
}

// Shift + volume down sends volume up
static bool process_record_volume(uint16_t keycode, keyrecord_t *record) {
    static bool volume_up_held = false;
    if (keycode != KC_VOLD) {
        return true;
    }
    if (record->event.pressed && (get_mods() & MOD_MASK_SHIFT)) {
        register_code(KC_VOLU);
        volume_up_held = true;
        return false;
    }
    if (!record->event.pressed && volume_up_held) {
        unregister_code(KC_VOLU);
        volume_up_held = false;
        return false;
    }
    return true;
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        backlight_fx_activity(true);
    }
    return process_record_lock(keycode, record) &&
           process_record_trackball(keycode, record) &&
           process_record_gamepad(keycode, record) &&
           process_record_volume(keycode, record) &&
           process_record_user(keycode, record);
}

// VIA offers layer keys for layers 0-9 (MO/TG/TO/DF/...), but only the first
// DYNAMIC_KEYMAP_LAYER_COUNT layers exist: on the others every key is KC_NO,
// so e.g. TG(5) would leave a dead keyboard. Ignore layers that don't exist.
#define EXISTING_LAYERS_MASK ((layer_state_t)((1UL << DYNAMIC_KEYMAP_LAYER_COUNT) - 1))

// The active (highest) layer is reported on the QMK console as
// "uconsole:layer N" on every change and every LAYER_REPORT_INTERVAL ms, so a
// listener started later (tools/uconsole-layer) learns it too. The console is
// its own HID interface (usage page 0xFF31), separate from VIA's raw HID.
#define LAYER_REPORT_INTERVAL 5000

static uint8_t  reported_layer = UINT8_MAX;
static uint32_t last_layer_report = 0;

static void report_layer(layer_state_t layers, layer_state_t default_layers) {
    const uint8_t layer = get_highest_layer(layers | default_layers);
    if (layer != reported_layer || timer_elapsed32(last_layer_report) >= LAYER_REPORT_INTERVAL) {
        uprintf("uconsole:layer %u\n", layer);
        reported_layer    = layer;
        last_layer_report = timer_read32();
    }
}

layer_state_t layer_state_set_kb(layer_state_t state) {
    state = layer_state_set_user(state & EXISTING_LAYERS_MASK);
    report_layer(state, default_layer_state);
    return state;
}

layer_state_t default_layer_state_set_kb(layer_state_t state) {
    state &= EXISTING_LAYERS_MASK;
    state = default_layer_state_set_user(state ? state : 1); // keep layer 0
    report_layer(layer_state, state);
    return state;
}

void housekeeping_task_kb(void) {
    backlight_fx_task();
    report_layer(layer_state, default_layer_state);
    housekeeping_task_user();
}

#ifdef VIA_ENABLE
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    uint8_t *command_id = &data[0];
    // VIA sends save as [ command_id, channel_id ] only, without a value id
    if (*command_id == id_custom_save && data[1] == id_custom_channel) {
        trackball_config_save();
        backlight_fx_config_save();
        return;
    }
    if (data[1] != id_custom_channel ||
        !(trackball_via_command(*command_id, data[2], &data[3]) ||
          backlight_fx_via_command(*command_id, data[2], &data[3]))) {
        *command_id = id_unhandled;
    }
}
#endif

void mcu_reset(void) {
    clear_bootloader_flag();
    NVIC_SystemReset();
}

void bootloader_jump(void) {
    clear_bootloader_flag();
    NVIC_SystemReset();
}
