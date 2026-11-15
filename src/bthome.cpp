#include "bthome.h"

#include <string.h>

namespace {
constexpr uint8_t kAdTypeFlags = 0x01;
constexpr uint8_t kBleFlagsLeGenDiscNoBredr = 0x06;
constexpr uint8_t kAdTypeCompleteLocalName = 0x09;
constexpr uint8_t kAdTypeServiceData16 = 0x16;

constexpr uint8_t kBthomeUuidLsb = 0xD2; // 0xFCD2 little-endian
constexpr uint8_t kBthomeUuidMsb = 0xFC;
constexpr uint8_t kBthomeDevInfoV2Trigger = 0x44; // Version 2, unencrypted, trigger-based

constexpr uint8_t kObjPacketId = 0x00;
constexpr uint8_t kObjBatteryPct = 0x01;
constexpr uint8_t kObjButton = 0x3A;
} // namespace

size_t bthome_build_adv_payload(
    uint8_t *out_buf,
    uint8_t packet_id,
    uint8_t battery_pct,
    const BthomeButtonEvent events[BTHOME_NUM_BUTTONS],
    const char *device_name)
{
    if (!out_buf || !events) {
        return 0;
    }

    if (battery_pct > 100) {
        battery_pct = 100;
    }

    size_t idx = 0;

    // 1. BLE Flags (3 bytes)
    out_buf[idx++] = 0x02;
    out_buf[idx++] = kAdTypeFlags;
    out_buf[idx++] = kBleFlagsLeGenDiscNoBredr;

    // 2. BTHome v2 Service Data (17 bytes total: 1 len + 16 payload)
    // Object IDs are strictly in ascending order: 0x00, 0x01, 0x3A x 4
    out_buf[idx++] = 16;
    out_buf[idx++] = kAdTypeServiceData16;
    out_buf[idx++] = kBthomeUuidLsb;
    out_buf[idx++] = kBthomeUuidMsb;
    out_buf[idx++] = kBthomeDevInfoV2Trigger;
    out_buf[idx++] = kObjPacketId;
    out_buf[idx++] = packet_id;
    out_buf[idx++] = kObjBatteryPct;
    out_buf[idx++] = battery_pct;

    for (size_t i = 0; i < BTHOME_NUM_BUTTONS; ++i) {
        out_buf[idx++] = kObjButton;
        out_buf[idx++] = static_cast<uint8_t>(events[i]);
    }

    // 3. Complete Local Name (optional, fits in remaining space up to 31 bytes)
    if (device_name && device_name[0] != '\0') {
        size_t remaining = BTHOME_MAX_ADV_LEN - idx;
        if (remaining >= 3) {
            size_t max_name_len = remaining - 2;
            size_t name_len = strlen(device_name);
            if (name_len > max_name_len) {
                name_len = max_name_len;
            }
            out_buf[idx++] = static_cast<uint8_t>(name_len + 1);
            out_buf[idx++] = kAdTypeCompleteLocalName;
            memcpy(&out_buf[idx], device_name, name_len);
            idx += name_len;
        }
    }

    return idx;
}

