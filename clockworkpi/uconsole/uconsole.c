#include "quantum.h"
#include "trackball.h"
#include "backlight_fx.h"
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

void housekeeping_task_kb(void) {
    backlight_fx_task();
    housekeeping_task_user();
}

#ifdef VIA_ENABLE
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    uint8_t *command_id = &data[0];
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
