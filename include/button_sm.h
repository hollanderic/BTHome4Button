#pragma once

#include <stdint.h>

#include "bthome.h"

namespace btn_timing {
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t LONG_PRESS_MS = 700;
constexpr uint32_t DOUBLE_PRESS_WINDOW_MS = 300;
} // namespace btn_timing

/**
 * Independent per-button state machine supporting:
 *   - Single press
 *   - Double press
 *   - Long press
 *
 * Designed for interrupt-driven / RTC-driven low-power sleep:
 * call update() on GPIO edge or timer expiry, and use next_timeout_ms()
 * to schedule the next RTC wakeup.
 */
class ButtonStateMachine {
public:
    enum class State : uint8_t {
        IDLE,
        DEBOUNCE_PRESS,
        PRESSED,
        DEBOUNCE_RELEASE,
        WAIT_DOUBLE,
        DEBOUNCE_SECOND_PRESS,
        SECOND_PRESSED,
        DEBOUNCE_SECOND_RELEASE,
        WAIT_RELEASE_AFTER_LONG,
        DEBOUNCE_LONG_RELEASE,
    };

    ButtonStateMachine() = default;

    /**
     * Seed the state machine when waking from System OFF via this button.
     * Skips initial DEBOUNCE_PRESS since the wake-up DETECT already occurred
     * before application startup.
     */
    void on_system_off_wake(uint32_t now_ms, bool still_pressed);

    /**
     * Advance the state machine.
     *
     * @param now_ms       Current monotonic timestamp in milliseconds.
     * @param raw_pressed  Current debounced-or-raw physical button state (true = pressed).
     * @return Detected BthomeButtonEvent, or BthomeButtonEvent::NONE.
     */
    BthomeButtonEvent update(uint32_t now_ms, bool raw_pressed);

    /**
     * Returns true when the button is completely idle and ready for System OFF.
     */
    bool is_idle() const { return state_ == State::IDLE; }

    /**
     * Returns milliseconds until the next time-based transition, or UINT32_MAX
     * if the state machine is waiting solely on a GPIO edge.
     */
    uint32_t next_timeout_ms(uint32_t now_ms) const;

    State state() const { return state_; }

private:
    State state_ = State::IDLE;
    uint32_t state_since_ms_ = 0;
    uint32_t press_start_ms_ = 0;
    uint32_t wait_double_start_ms_ = 0;
};

