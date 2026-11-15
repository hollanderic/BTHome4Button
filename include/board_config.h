#pragma once

#include <stdint.h>

/**
 * Hardware configuration for PCA10040 (nRF52-DK) and the custom CR2032 PCB.
 * Pin numbers are raw nRF52832 Port 0 GPIO numbers (P0.xx).
 */

#define NUM_BUTTONS 4

#if defined(BOARD_CUSTOM_PCB)

// TODO: Update these GPIO assignments to match your custom PCB schematic.
#define BTN_PIN_0 13
#define BTN_PIN_1 14
#define BTN_PIN_2 15
#define BTN_PIN_3 16
#define BTN_ACTIVE_LOW 1

#define LED_STATUS_PIN 17
#define LED_ACTIVE_LOW 1

// Set to 1 if L2/L3 DC/DC inductors are populated on the custom PCB.
#define BOARD_HAS_DCDC 1

// Set to 1 if a 32.768 kHz crystal (XL1/XL2) is populated; 0 uses internal LFRC.
#define BOARD_HAS_LFXTAL 0

#elif defined(BOARD_PICOMICRO_NRF52840)

// PicoMicro / Pro Micro nRF52840: Buttons 1..4 on P0.17, P0.20, P0.22, P0.24
// Active high with internal pull-down resistors.
#define BTN_PIN_0 17
#define BTN_PIN_1 20
#define BTN_PIN_2 22
#define BTN_PIN_3 24
#define BTN_ACTIVE_LOW 0

// Onboard blue status LED on Pro Micro / PicoMicro nRF52840 (P0.15, active high)
#define LED_STATUS_PIN 15
#define LED_ACTIVE_LOW 0

#define BOARD_HAS_DCDC 1
#define BOARD_HAS_LFXTAL 1

// Maximum TX power on nRF52840 (+8 dBm) to compensate for compact antenna
#define BLE_TX_POWER_REG 0x08UL

#else // BOARD_PCA10040 (default)

// PCA10040: Buttons 1..4 are P0.13..P0.16 (active low, internal pull-up)
#define BTN_PIN_0 13
#define BTN_PIN_1 14
#define BTN_PIN_2 15
#define BTN_PIN_3 16
#define BTN_ACTIVE_LOW 1

// PCA10040: LED1 is P0.17 (active low)
#define LED_STATUS_PIN 17
#define LED_ACTIVE_LOW 1

// PCA10040 includes DC/DC inductors and 32.768 kHz LFXTAL
#define BOARD_HAS_DCDC 1
#define BOARD_HAS_LFXTAL 1

#endif

static const uint8_t kButtonPins[NUM_BUTTONS] = {
    BTN_PIN_0,
    BTN_PIN_1,
    BTN_PIN_2,
    BTN_PIN_3,
};

// Device name advertised in BTHome packets (max 8 chars to fit 31-byte ADV PDU)
#define BTBUTTON_DEVICE_NAME "BTButton"

// Number of redundant BLE advertisement rounds (ch 37, 38, 39) per event
// 10 rounds @ 27 ms (~243 ms span) avoids phase-locking with 100ms/200ms BLE scanners
#define BLE_ADV_BURST_COUNT 10
#define BLE_ADV_BURST_INTERVAL_MS 27

// BLE TX power register value (0 dBm balances range and CR2032 peak current)
// Options: 0x08 (+8dBm, nRF52840 only), 0x04 (+4dBm), 0x00 (0dBm), 0xFC (-4dBm), 0xF8 (-8dBm)
#ifndef BLE_TX_POWER_REG
#define BLE_TX_POWER_REG 0x00UL
#endif

