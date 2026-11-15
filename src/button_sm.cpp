#include "button_sm.h"

namespace {
uint32_t remaining_ms(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms)
{
    uint32_t elapsed = now_ms - start_ms;
    return (elapsed >= duration_ms) ? 0 : (duration_ms - elapsed);
}
} // namespace

void ButtonStateMachine::on_system_off_wake(uint32_t now_ms, bool still_pressed)
{
    if (still_pressed) {
        state_ = State::PRESSED;
        press_start_ms_ = now_ms;
    } else {
        state_ = State::WAIT_DOUBLE;
        wait_double_start_ms_ = now_ms;
    }
}

BthomeButtonEvent ButtonStateMachine::update(uint32_t now_ms, bool raw_pressed)
{
    switch (state_) {
    case State::IDLE:
        if (raw_pressed) {
            state_ = State::DEBOUNCE_PRESS;
            state_since_ms_ = now_ms;
        }
        break;

    case State::DEBOUNCE_PRESS:
        if (!raw_pressed) {
            state_ = State::IDLE;
        } else if ((now_ms - state_since_ms_) >= btn_timing::DEBOUNCE_MS) {
            state_ = State::PRESSED;
            press_start_ms_ = now_ms;
        }
        break;

    case State::PRESSED:
        if (!raw_pressed) {
            state_ = State::DEBOUNCE_RELEASE;
            state_since_ms_ = now_ms;
        } else if ((now_ms - press_start_ms_) >= btn_timing::LONG_PRESS_MS) {
            state_ = State::WAIT_RELEASE_AFTER_LONG;
            return BthomeButtonEvent::LONG_PRESS;
        }
        break;

    case State::DEBOUNCE_RELEASE:
        if (raw_pressed) {
            state_ = State::PRESSED;
        } else if ((now_ms - state_since_ms_) >= btn_timing::DEBOUNCE_MS) {
            state_ = State::WAIT_DOUBLE;
            wait_double_start_ms_ = now_ms;
        }
        break;

    case State::WAIT_DOUBLE:
        if (raw_pressed) {
            state_ = State::DEBOUNCE_SECOND_PRESS;
            state_since_ms_ = now_ms;
        } else if ((now_ms - wait_double_start_ms_) >= btn_timing::DOUBLE_PRESS_WINDOW_MS) {
            state_ = State::IDLE;
            return BthomeButtonEvent::PRESS;
        }
        break;

    case State::DEBOUNCE_SECOND_PRESS:
        if (!raw_pressed) {
            state_ = State::WAIT_DOUBLE;
        } else if ((now_ms - state_since_ms_) >= btn_timing::DEBOUNCE_MS) {
            state_ = State::SECOND_PRESSED;
            press_start_ms_ = now_ms;
        }
        break;

    case State::SECOND_PRESSED:
        if (!raw_pressed) {
            state_ = State::DEBOUNCE_SECOND_RELEASE;
            state_since_ms_ = now_ms;
        } else if ((now_ms - press_start_ms_) >= btn_timing::LONG_PRESS_MS) {
            state_ = State::WAIT_RELEASE_AFTER_LONG;
            return BthomeButtonEvent::LONG_PRESS;
        }
        break;

    case State::DEBOUNCE_SECOND_RELEASE:
        if (raw_pressed) {
            state_ = State::SECOND_PRESSED;
        } else if ((now_ms - state_since_ms_) >= btn_timing::DEBOUNCE_MS) {
            state_ = State::IDLE;
            return BthomeButtonEvent::DOUBLE_PRESS;
        }
        break;

    case State::WAIT_RELEASE_AFTER_LONG:
        if (!raw_pressed) {
            state_ = State::DEBOUNCE_LONG_RELEASE;
            state_since_ms_ = now_ms;
        }
        break;

    case State::DEBOUNCE_LONG_RELEASE:
        if (raw_pressed) {
            state_ = State::WAIT_RELEASE_AFTER_LONG;
        } else if ((now_ms - state_since_ms_) >= btn_timing::DEBOUNCE_MS) {
            state_ = State::IDLE;
        }
        break;
    }

    return BthomeButtonEvent::NONE;
}

uint32_t ButtonStateMachine::next_timeout_ms(uint32_t now_ms) const
{
    switch (state_) {
    case State::IDLE:
    case State::WAIT_RELEASE_AFTER_LONG:
        return UINT32_MAX;

    case State::DEBOUNCE_PRESS:
    case State::DEBOUNCE_RELEASE:
    case State::DEBOUNCE_SECOND_PRESS:
    case State::DEBOUNCE_SECOND_RELEASE:
    case State::DEBOUNCE_LONG_RELEASE:
        return remaining_ms(now_ms, state_since_ms_, btn_timing::DEBOUNCE_MS);

    case State::PRESSED:
    case State::SECOND_PRESSED:
        return remaining_ms(now_ms, press_start_ms_, btn_timing::LONG_PRESS_MS);

    case State::WAIT_DOUBLE:
        return remaining_ms(now_ms, wait_double_start_ms_, btn_timing::DOUBLE_PRESS_WINDOW_MS);
    }

    return UINT32_MAX;
}

