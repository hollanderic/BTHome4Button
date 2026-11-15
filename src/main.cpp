#include <Arduino.h>

#include "ble_adv.h"
#include "board_config.h"
#include "bthome.h"
#include "button_sm.h"
#include "power_hal.h"

namespace {
void broadcast_bthome_events(
    const BthomeButtonEvent events[NUM_BUTTONS],
    uint8_t battery_pct)
{
    uint8_t adv_buf[BTHOME_MAX_ADV_LEN];
    uint8_t packet_id = power_hal_next_packet_id();

    size_t adv_len = bthome_build_adv_payload(
        adv_buf,
        packet_id,
        battery_pct,
        events,
        BTBUTTON_DEVICE_NAME);

    ble_adv_send_burst(
        adv_buf,
        adv_len,
        BLE_ADV_BURST_COUNT,
        BLE_ADV_BURST_INTERVAL_MS);
}
} // namespace

void setup()
{
    power_hal_init();

    uint8_t battery_pct = power_hal_read_battery_pct();
    ButtonStateMachine buttons[NUM_BUTTONS];
    uint32_t boot_ms = power_hal_now_ms();
    bool any_wake_btn = false;
#if defined(BOARD_PICOMICRO_NRF52840)
    constexpr uint32_t kBootloaderHoldMs = 3000;
    bool dfu_combo_active = false;
    uint32_t dfu_combo_start_ms = 0;
#endif

    for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
        bool pressed = power_hal_read_button(i);
        bool latched = power_hal_was_button_latched(i);
        if (pressed || latched) {
            buttons[i].on_system_off_wake(boot_ms, pressed);
            any_wake_btn = true;
        }
    }

    // If booted without any button pressed or latched (e.g. battery insertion or reset),
    // blink status LED once, send a discovery packet, and enter System OFF.
    if (!any_wake_btn) {
        power_hal_set_led(true);
        BthomeButtonEvent init_events[NUM_BUTTONS] = {
            BthomeButtonEvent::NONE,
            BthomeButtonEvent::NONE,
            BthomeButtonEvent::NONE,
            BthomeButtonEvent::NONE,
        };
        broadcast_bthome_events(init_events, battery_pct);
        power_hal_set_led(false);
        power_hal_enter_system_off();
    }

    for (;;) {
        uint32_t now_ms = power_hal_now_ms();
        BthomeButtonEvent pending_events[NUM_BUTTONS];
        bool has_event = false;
        bool all_idle = true;
        uint32_t min_timeout_ms = UINT32_MAX;

        for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
            bool pressed = power_hal_read_button(i);
            pending_events[i] = buttons[i].update(now_ms, pressed);
            if (pending_events[i] != BthomeButtonEvent::NONE) {
                has_event = true;
            }
            if (!buttons[i].is_idle() || pressed) {
                all_idle = false;
            }
            uint32_t t = buttons[i].next_timeout_ms(now_ms);
            if (t < min_timeout_ms) {
                min_timeout_ms = t;
            }
        }

        if (has_event) {
            power_hal_set_led(true);
            broadcast_bthome_events(pending_events, battery_pct);
            power_hal_set_led(false);
            // Re-evaluate immediately after burst in case a button changed during TX
            continue;
        }

#if defined(BOARD_PICOMICRO_NRF52840)
        // Enter USB DFU bootloader if buttons 1 (idx 0) and 4 (idx 3) are held together for 3s
        if (power_hal_read_button(0) && power_hal_read_button(3)) {
            if (!dfu_combo_active) {
                dfu_combo_active = true;
                dfu_combo_start_ms = now_ms;
            }
            uint32_t held_ms = now_ms - dfu_combo_start_ms;
            if (held_ms >= kBootloaderHoldMs) {
                power_hal_enter_bootloader();
            }
            uint32_t rem_ms = kBootloaderHoldMs - held_ms;
            if (rem_ms < min_timeout_ms) {
                min_timeout_ms = rem_ms;
            }
        } else {
            dfu_combo_active = false;
        }
#endif

        if (all_idle) {
            power_hal_enter_system_off();
        }

        power_hal_sleep_ms(min_timeout_ms);
    }
}

void loop()
{
    // Unreachable: setup() always enters System OFF when idle.
}

