#include "power_hal.h"

#include <nrf.h>

namespace {
constexpr uint32_t kRtcFreqHz = 1024; // 32768 Hz / (PRESCALER 31 + 1)

volatile bool g_rtc2_fired = false;
uint32_t g_wake_latch = 0;

void configure_button_pin_input(uint8_t pin, uint32_t sense_mode)
{
    uint32_t pull = BTN_ACTIVE_LOW ? GPIO_PIN_CNF_PULL_Pullup : GPIO_PIN_CNF_PULL_Pulldown;
    NRF_P0->PIN_CNF[pin] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
        (pull << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (sense_mode << GPIO_PIN_CNF_SENSE_Pos);
}

void arm_button_change_sense()
{
    for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
        uint8_t pin = kButtonPins[i];
        bool is_high = ((NRF_P0->IN >> pin) & 1UL) != 0;
        uint32_t sense = is_high ? GPIO_PIN_CNF_SENSE_Low : GPIO_PIN_CNF_SENSE_High;
        configure_button_pin_input(pin, sense);
    }
}
} // namespace

extern "C" void RTC2_IRQHandler(void)
{
    if (NRF_RTC2->EVENTS_COMPARE[0]) {
        NRF_RTC2->EVENTS_COMPARE[0] = 0;
        g_rtc2_fired = true;
    }
}

void power_hal_init()
{
    // Capture and clear GPIO LATCH before reconfiguring pins
    g_wake_latch = NRF_P0->LATCH;
    NRF_P0->LATCH = g_wake_latch;

#if BOARD_HAS_DCDC
    NRF_POWER->DCDCEN = POWER_DCDCEN_DCDCEN_Enabled << POWER_DCDCEN_DCDCEN_Pos;
#endif

    // Stop Arduino core's RTC1 so it does not generate background wakeups
    NRF_RTC1->TASKS_STOP = 1;
    NRF_RTC1->INTENCLR = 0xFFFFFFFFUL;
    NVIC_DisableIRQ(RTC1_IRQn);

#if defined(NRF_USBD)
    // Disable USB device peripheral & IRQ started by Adafruit core's TinyUSB init
    // so USB enumeration/interrupts cannot preempt BLE TX bursts or burn power.
    NVIC_DisableIRQ(USBD_IRQn);
    NVIC_ClearPendingIRQ(USBD_IRQn);
    NRF_USBD->ENABLE = 0;
#endif

    // Start LFCLK if not already running
    if ((NRF_CLOCK->LFCLKSTAT & CLOCK_LFCLKSTAT_STATE_Msk) == 0) {
#if BOARD_HAS_LFXTAL
        NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos;
#else
        NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos;
#endif
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
        NRF_CLOCK->TASKS_LFCLKSTART = 1;
        while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
        }
        NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    }

    // Configure RTC2 @ 1024 Hz for low-power timing
    NRF_RTC2->TASKS_STOP = 1;
    NRF_RTC2->TASKS_CLEAR = 1;
    NRF_RTC2->PRESCALER = 31; // 32768 / (31 + 1) = 1024 Hz
    NRF_RTC2->INTENCLR = 0xFFFFFFFFUL;
    NRF_RTC2->EVENTS_COMPARE[0] = 0;
    NVIC_ClearPendingIRQ(RTC2_IRQn);
    NVIC_SetPriority(RTC2_IRQn, 3);
    NVIC_EnableIRQ(RTC2_IRQn);
    NRF_RTC2->TASKS_START = 1;

    // Configure 4 independent button inputs
    for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
        configure_button_pin_input(kButtonPins[i], GPIO_PIN_CNF_SENSE_Disabled);
    }

    // Use SEVONPEND so GPIOTE PORT events wake __WFE() via NVIC pending bit
    // without requiring a GPIOTE_IRQHandler (avoids collision with Adafruit BSP).
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;
    NVIC_DisableIRQ(GPIOTE_IRQn);
    NRF_GPIOTE->EVENTS_PORT = 0;
    NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Msk;
    NVIC_ClearPendingIRQ(GPIOTE_IRQn);

    // Configure status LED pin (initially off)
    power_hal_set_led(false);
    NRF_P0->PIN_CNF[LED_STATUS_PIN] =
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

uint32_t power_hal_now_ms()
{
    uint32_t ticks = NRF_RTC2->COUNTER;
    return static_cast<uint32_t>((static_cast<uint64_t>(ticks) * 1000ULL) / kRtcFreqHz);
}

bool power_hal_read_button(uint8_t idx)
{
    if (idx >= NUM_BUTTONS) {
        return false;
    }
    bool high = ((NRF_P0->IN >> kButtonPins[idx]) & 1UL) != 0;
    return BTN_ACTIVE_LOW ? !high : high;
}

bool power_hal_was_button_latched(uint8_t idx)
{
    if (idx >= NUM_BUTTONS) {
        return false;
    }
    return ((g_wake_latch >> kButtonPins[idx]) & 1UL) != 0;
}

void power_hal_set_led(bool on)
{
    bool drive_high = LED_ACTIVE_LOW ? !on : on;
    if (drive_high) {
        NRF_P0->OUTSET = (1UL << LED_STATUS_PIN);
    } else {
        NRF_P0->OUTCLR = (1UL << LED_STATUS_PIN);
    }
}

void power_hal_sleep_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return;
    }

    g_rtc2_fired = false;

    arm_button_change_sense();
    NRF_GPIOTE->EVENTS_PORT = 0;
    NVIC_ClearPendingIRQ(GPIOTE_IRQn);

    if (timeout_ms != UINT32_MAX) {
        uint32_t ticks = static_cast<uint32_t>((static_cast<uint64_t>(timeout_ms) * kRtcFreqHz + 999ULL) / 1000ULL);
        if (ticks < 2) {
            ticks = 2; // nRF52 RTC requires CC >= COUNTER + 2 for reliable match
        }
        uint32_t target = (NRF_RTC2->COUNTER + ticks) & 0x00FFFFFFUL;
        NRF_RTC2->EVENTS_COMPARE[0] = 0;
        NRF_RTC2->CC[0] = target;
        NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;
    }

    while (!NRF_GPIOTE->EVENTS_PORT && !g_rtc2_fired) {
        __SEV();
        __WFE();
        __WFE();
    }

    NRF_GPIOTE->EVENTS_PORT = 0;
    NVIC_ClearPendingIRQ(GPIOTE_IRQn);
    NRF_RTC2->INTENCLR = RTC_INTENCLR_COMPARE0_Msk;
    for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
        configure_button_pin_input(kButtonPins[i], GPIO_PIN_CNF_SENSE_Disabled);
    }
}

uint8_t power_hal_read_battery_pct()
{
    static volatile int16_t saadc_sample = 0;

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_10bit << SAADC_RESOLUTION_VAL_Pos;
    NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDD << SAADC_CH_PSELP_PSELP_Pos;
    NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC << SAADC_CH_PSELN_PSELN_Pos;
    NRF_SAADC->CH[0].CONFIG =
        (SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) |
        (SAADC_CH_CONFIG_RESN_Bypass << SAADC_CH_CONFIG_RESN_Pos) |
        (SAADC_CH_CONFIG_GAIN_Gain1_6 << SAADC_CH_CONFIG_GAIN_Pos) |
        (SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos) |
        (SAADC_CH_CONFIG_TACQ_10us << SAADC_CH_CONFIG_TACQ_Pos) |
        (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);

    NRF_SAADC->RESULT.PTR = reinterpret_cast<uint32_t>(&saadc_sample);
    NRF_SAADC->RESULT.MAXCNT = 1;

    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled << SAADC_ENABLE_ENABLE_Pos;
    NRF_SAADC->EVENTS_STARTED = 0;
    NRF_SAADC->TASKS_START = 1;
    while (NRF_SAADC->EVENTS_STARTED == 0) {
    }

    NRF_SAADC->EVENTS_END = 0;
    NRF_SAADC->TASKS_SAMPLE = 1;
    while (NRF_SAADC->EVENTS_END == 0) {
    }

    NRF_SAADC->TASKS_STOP = 1;
    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Disabled << SAADC_ENABLE_ENABLE_Pos;

    int32_t raw = saadc_sample;
    if (raw < 0) {
        raw = 0;
    }
    // 10-bit full scale = 0.6V * 6 = 3.6V = 3600 mV
    int32_t mv = (raw * 3600) / 1024;

    // CR2032 approximate usable range: 2000 mV (0%) .. 3000 mV (100%)
    if (mv >= 3000) {
        return 100;
    }
    if (mv <= 2000) {
        return 0;
    }
    return static_cast<uint8_t>((mv - 2000) / 10);
}

uint8_t power_hal_next_packet_id()
{
#if defined(BOARD_PICOMICRO_NRF52840) && defined(POWER_GPREGRET2_GPREGRET_Msk)
    // Use GPREGRET2 on nRF52840 so GPREGRET stays reserved for bootloader DFU magic
    uint8_t next_id = static_cast<uint8_t>((NRF_POWER->GPREGRET2 + 1U) & 0xFFU);
    NRF_POWER->GPREGRET2 = next_id;
#else
    uint8_t next_id = static_cast<uint8_t>((NRF_POWER->GPREGRET + 1U) & 0xFFU);
    NRF_POWER->GPREGRET = next_id;
#endif
    return next_id;
}

[[noreturn]] void power_hal_enter_system_off()
{
    power_hal_set_led(false);

    NRF_RTC2->TASKS_STOP = 1;
    NRF_RTC2->INTENCLR = 0xFFFFFFFFUL;
    NRF_GPIOTE->INTENCLR = 0xFFFFFFFFUL;

    // Arm button pins to wake from System OFF on press
    uint32_t wake_sense = BTN_ACTIVE_LOW ? GPIO_PIN_CNF_SENSE_Low : GPIO_PIN_CNF_SENSE_High;
    for (uint8_t i = 0; i < NUM_BUTTONS; ++i) {
        configure_button_pin_input(kButtonPins[i], wake_sense);
    }

    NRF_GPIOTE->EVENTS_PORT = 0;
    NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter << POWER_SYSTEMOFF_SYSTEMOFF_Pos;
    __DSB();

    for (;;) {
        __WFE();
    }
}

#if defined(BOARD_PICOMICRO_NRF52840)
[[noreturn]] void power_hal_enter_bootloader()
{
    // Brief LED pulse to confirm bootloader entry
    power_hal_set_led(true);
    power_hal_sleep_ms(150);
    power_hal_set_led(false);

    // DFU_MAGIC_UF2_RESET (0x57) enables both UF2 and Serial DFU in Adafruit bootloader
    NRF_POWER->GPREGRET = 0x57UL;
    __DSB();
    NVIC_SystemReset();
    for (;;) {
    }
}
#endif

