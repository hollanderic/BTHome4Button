#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"

/**
 * Initialize low-power clocks (LFCLK + RTC2), DC/DC, button GPIOs, and status LED.
 */
void power_hal_init();

/**
 * Current monotonic time in milliseconds since boot/wake (driven by RTC2 @ 1024 Hz).
 */
uint32_t power_hal_now_ms();

/**
 * Read raw pressed state for button index (0 .. NUM_BUTTONS-1).
 */
bool power_hal_read_button(uint8_t idx);

/**
 * Returns true if button index (0 .. NUM_BUTTONS-1) was latched in GPIO LATCH
 * on wake-up from System OFF.
 */
bool power_hal_was_button_latched(uint8_t idx);

/**
 * Set status LED on or off.
 */
void power_hal_set_led(bool on);

/**
 * Enter System ON low-power sleep (__WFE) until either:
 *   - Any of the 4 buttons changes state (GPIOTE PORT event), OR
 *   - timeout_ms elapses (if timeout_ms != UINT32_MAX).
 */
void power_hal_sleep_ms(uint32_t timeout_ms);

/**
 * Measure CR2032 VDD voltage via SAADC and return estimated battery percentage (0..100).
 * SAADC is powered down immediately after the single conversion.
 */
uint8_t power_hal_read_battery_pct();

/**
 * Return the next 8-bit BTHome Packet ID, persisted across System OFF in GPREGRET.
 */
uint8_t power_hal_next_packet_id();

/**
 * Configure button wake-up SENSE and enter nRF52832/nRF52840 System OFF (~0.3 uA).
 * Does not return; next button press resets the MCU.
 */
[[noreturn]] void power_hal_enter_system_off();

#if defined(BOARD_PICOMICRO_NRF52840)
/**
 * Reboot into the Adafruit UF2/Serial DFU bootloader for USB reflashing.
 */
[[noreturn]] void power_hal_enter_bootloader();
#endif

