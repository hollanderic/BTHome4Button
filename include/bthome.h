#pragma once

#include <stddef.h>
#include <stdint.h>

#define BTHOME_MAX_ADV_LEN 31
#define BTHOME_NUM_BUTTONS 4

/**
 * BTHome v2 Button object (0x3A) event codes.
 */
enum class BthomeButtonEvent : uint8_t {
    NONE = 0x00,
    PRESS = 0x01,
    DOUBLE_PRESS = 0x02,
    TRIPLE_PRESS = 0x03,
    LONG_PRESS = 0x04,
};

/**
 * Build a BTHome v2 BLE advertising payload (for ADV_NONCONN_IND).
 *
 * Layout (up to 31 bytes):
 *   [3B]  Flags: 0x02, 0x01, 0x06
 *   [17B] Service Data (UUID 0xFCD2, dev_info 0x44):
 *           0x00 <packet_id>
 *           0x01 <battery_pct>
 *           0x3A <btn0>  0x3A <btn1>  0x3A <btn2>  0x3A <btn3>
 *   [<=11B] Complete Local Name (0x09), truncated if needed to fit 31 bytes.
 *
 * @param out_buf      Destination buffer (must be at least BTHOME_MAX_ADV_LEN bytes).
 * @param packet_id    Monotonically wrapping 8-bit packet ID for deduplication.
 * @param battery_pct  Battery percentage (clamped to 0..100).
 * @param events       Array of BTHOME_NUM_BUTTONS button events.
 * @param device_name  Optional null-terminated device name (may be nullptr).
 * @return Total payload length in bytes (<= 31).
 */
size_t bthome_build_adv_payload(
    uint8_t *out_buf,
    uint8_t packet_id,
    uint8_t battery_pct,
    const BthomeButtonEvent events[BTHOME_NUM_BUTTONS],
    const char *device_name);

