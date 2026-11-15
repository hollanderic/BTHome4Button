#include "ble_adv.h"

#include <string.h>
#include <nrf.h>

#include "board_config.h"
#include "power_hal.h"

namespace {
struct BleChannel {
    uint8_t channel;
    uint8_t freq_offset; // 2400 MHz + freq_offset
};

constexpr BleChannel kAdvChannels[3] = {
    {37, 2},   // 2402 MHz
    {38, 26},  // 2426 MHz
    {39, 80},  // 2480 MHz
};

constexpr uint8_t kPduTypeAdvNonconnIndTxAddRandom = 0x42; // Type=2 (ADV_NONCONN_IND), TxAdd=1
constexpr size_t kBleAddrLen = 6;
constexpr size_t kMaxAdvDataLen = 31;

void start_hfxo()
{
    if ((NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_SRC_Msk) ==
        (CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos)) {
        return;
    }
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {
    }
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
}

void stop_hfxo()
{
    NRF_CLOCK->TASKS_HFCLKSTOP = 1;
}

void load_static_random_mac(uint8_t mac_out[kBleAddrLen])
{
    uint32_t lo = NRF_FICR->DEVICEADDR[0];
    uint32_t hi = NRF_FICR->DEVICEADDR[1];
    mac_out[0] = static_cast<uint8_t>(lo & 0xFFU);
    mac_out[1] = static_cast<uint8_t>((lo >> 8) & 0xFFU);
    mac_out[2] = static_cast<uint8_t>((lo >> 16) & 0xFFU);
    mac_out[3] = static_cast<uint8_t>((lo >> 24) & 0xFFU);
    mac_out[4] = static_cast<uint8_t>(hi & 0xFFU);
    mac_out[5] = static_cast<uint8_t>(((hi >> 8) & 0xFFU) | 0xC0U); // Static Random Address
}

void configure_radio_ble_adv()
{
    NRF_RADIO->POWER = RADIO_POWER_POWER_Enabled << RADIO_POWER_POWER_Pos;

    NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
    NRF_RADIO->TXPOWER = BLE_TX_POWER_REG;

    NRF_RADIO->PCNF0 =
        (8UL << RADIO_PCNF0_LFLEN_Pos) |
        (1UL << RADIO_PCNF0_S0LEN_Pos) |
        (0UL << RADIO_PCNF0_S1LEN_Pos) |
        (RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos);

    NRF_RADIO->PCNF1 =
        ((kBleAddrLen + kMaxAdvDataLen) << RADIO_PCNF1_MAXLEN_Pos) |
        (0UL << RADIO_PCNF1_STATLEN_Pos) |
        (3UL << RADIO_PCNF1_BALEN_Pos) |
        (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) |
        (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);

    // BLE Advertising Access Address: 0x8E89BED6
    NRF_RADIO->BASE0 = 0x89BED600UL;
    NRF_RADIO->PREFIX0 = 0x8EUL;
    NRF_RADIO->TXADDRESS = 0x00UL;

    // 24-bit BLE CRC, skipping the access address
    NRF_RADIO->CRCCNF =
        (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) |
        (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
    NRF_RADIO->CRCPOLY = 0x00065BUL;
    NRF_RADIO->CRCINIT = 0x555555UL;

    NRF_RADIO->SHORTS =
        RADIO_SHORTS_READY_START_Msk |
        RADIO_SHORTS_END_DISABLE_Msk;
}
} // namespace

void ble_adv_send_burst(
    const uint8_t *adv_data,
    size_t adv_len,
    uint8_t burst_count,
    uint32_t interval_ms)
{
    if (!adv_data || adv_len == 0 || burst_count == 0) {
        return;
    }
    if (adv_len > kMaxAdvDataLen) {
        adv_len = kMaxAdvDataLen;
    }

    // S0 (1B) + Length (1B) + AdvA (6B) + AdvData (<=31B) = 39 bytes max
    static uint8_t pdu[2 + kBleAddrLen + kMaxAdvDataLen];
    pdu[0] = kPduTypeAdvNonconnIndTxAddRandom;
    pdu[1] = static_cast<uint8_t>(kBleAddrLen + adv_len);
    load_static_random_mac(&pdu[2]);
    memcpy(&pdu[2 + kBleAddrLen], adv_data, adv_len);

    for (uint8_t round = 0; round < burst_count; ++round) {
        start_hfxo();
        configure_radio_ble_adv();

        for (size_t ch = 0; ch < 3; ++ch) {
            NRF_RADIO->FREQUENCY = kAdvChannels[ch].freq_offset;
            NRF_RADIO->DATAWHITEIV = kAdvChannels[ch].channel;
            NRF_RADIO->PACKETPTR = reinterpret_cast<uint32_t>(pdu);

            NRF_RADIO->EVENTS_DISABLED = 0;
            NRF_RADIO->TASKS_TXEN = 1;
            while (NRF_RADIO->EVENTS_DISABLED == 0) {
            }
        }

        NRF_RADIO->POWER = RADIO_POWER_POWER_Disabled << RADIO_POWER_POWER_Pos;
        stop_hfxo();

        if (round + 1 < burst_count && interval_ms > 0) {
            power_hal_sleep_ms(interval_ms);
        }
    }
}

