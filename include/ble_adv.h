#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Broadcast a BLE ADV_NONCONN_IND packet burst on advertising channels 37, 38, and 39
 * using the nRF52832 RADIO peripheral directly (no SoftDevice required).
 *
 * Starts HFXO only for the duration of each TX burst and powers down RADIO & HFXO
 * immediately afterward for minimum energy consumption.
 *
 * @param adv_data      AD structures payload (up to 31 bytes).
 * @param adv_len       Length of adv_data in bytes.
 * @param burst_count   Number of 3-channel advertising rounds to transmit.
 * @param interval_ms   Low-power System ON sleep between rounds (ms).
 */
void ble_adv_send_burst(
    const uint8_t *adv_data,
    size_t adv_len,
    uint8_t burst_count,
    uint32_t interval_ms);

