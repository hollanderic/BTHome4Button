#include <assert.h>
#include <stdio.h>
#include <string.h>

// Include implementation directly for standalone native test binary
#include "../../src/bthome.cpp"
#include "../../src/button_sm.cpp"

static void test_bthome_packet_format()
{
    uint8_t buf[BTHOME_MAX_ADV_LEN];
    BthomeButtonEvent events[BTHOME_NUM_BUTTONS] = {
        BthomeButtonEvent::PRESS,
        BthomeButtonEvent::NONE,
        BthomeButtonEvent::DOUBLE_PRESS,
        BthomeButtonEvent::LONG_PRESS,
    };

    size_t len = bthome_build_adv_payload(buf, 0x2A, 87, events, "BTButton");
    assert(len == 30);

    const uint8_t expected[30] = {
        // Flags
        0x02, 0x01, 0x06,
        // Service Data (UUID 0xFCD2, v2 unencrypted trigger-based = 0x44)
        0x10, 0x16, 0xD2, 0xFC, 0x44,
        0x00, 0x2A,             // Packet ID = 42
        0x01, 87,               // Battery = 87%
        0x3A, 0x01,             // Button 0: PRESS
        0x3A, 0x00,             // Button 1: NONE
        0x3A, 0x02,             // Button 2: DOUBLE_PRESS
        0x3A, 0x04,             // Button 3: LONG_PRESS
        // Complete Local Name: "BTButton"
        0x09, 0x09, 'B', 'T', 'B', 'u', 't', 't', 'o', 'n',
    };
    assert(memcmp(buf, expected, sizeof(expected)) == 0);
}

static void test_bthome_clamping_and_truncation()
{
    uint8_t buf[BTHOME_MAX_ADV_LEN];
    BthomeButtonEvent events[BTHOME_NUM_BUTTONS] = {
        BthomeButtonEvent::NONE,
        BthomeButtonEvent::NONE,
        BthomeButtonEvent::NONE,
        BthomeButtonEvent::NONE,
    };

    size_t len = bthome_build_adv_payload(buf, 0xFF, 150, events, "VeryLongDeviceName");
    assert(len == BTHOME_MAX_ADV_LEN);
    assert(buf[11] == 100); // Battery clamped to 100%
    assert(buf[20] == 10);  // Name AD length = 1 (type) + 9 (truncated chars)
    assert(buf[21] == 0x09);
    assert(memcmp(&buf[22], "VeryLongD", 9) == 0);
}

static void test_button_bounce_rejected()
{
    ButtonStateMachine sm;
    assert(sm.is_idle());
    assert(sm.update(0, true) == BthomeButtonEvent::NONE);
    assert(sm.update(10, true) == BthomeButtonEvent::NONE);
    assert(sm.update(15, false) == BthomeButtonEvent::NONE); // Released before 25ms
    assert(sm.is_idle());
}

static void test_button_single_press()
{
    ButtonStateMachine sm;
    assert(sm.update(0, true) == BthomeButtonEvent::NONE);
    assert(sm.update(25, true) == BthomeButtonEvent::NONE);   // Debounced press
    assert(sm.update(100, false) == BthomeButtonEvent::NONE); // Release
    assert(sm.update(125, false) == BthomeButtonEvent::NONE); // Debounced release -> WAIT_DOUBLE
    assert(sm.update(424, false) == BthomeButtonEvent::NONE); // Still in 300ms window
    assert(sm.update(425, false) == BthomeButtonEvent::PRESS);
    assert(sm.is_idle());
}

static void test_button_double_press()
{
    ButtonStateMachine sm;
    // First press + release
    sm.update(0, true);
    sm.update(25, true);
    sm.update(80, false);
    sm.update(105, false); // Enters WAIT_DOUBLE
    // Second press within 300ms
    sm.update(200, true);
    sm.update(225, true);
    sm.update(280, false);
    assert(sm.update(305, false) == BthomeButtonEvent::DOUBLE_PRESS);
    assert(sm.is_idle());
}

static void test_button_long_press()
{
    ButtonStateMachine sm;
    sm.update(0, true);
    sm.update(25, true); // press_start_ms = 25
    assert(sm.update(724, true) == BthomeButtonEvent::NONE);
    assert(sm.update(725, true) == BthomeButtonEvent::LONG_PRESS);
    // Holding longer or releasing must not emit another event
    assert(sm.update(1500, true) == BthomeButtonEvent::NONE);
    assert(sm.update(1500, false) == BthomeButtonEvent::NONE);
    assert(sm.update(1525, false) == BthomeButtonEvent::NONE);
    assert(sm.is_idle());
}

static void test_four_independent_buttons()
{
    ButtonStateMachine btns[4];

    // Button 0: starts long press at t=0
    // Button 1: starts single press at t=10
    // Button 2: starts double press at t=20
    // Button 3: stays idle
    btns[0].update(0, true);
    btns[1].update(10, true);
    btns[2].update(20, true);

    btns[0].update(25, true);
    btns[1].update(35, true);
    btns[2].update(45, true);

    // Release btn 1 and btn 2
    btns[1].update(90, false);
    btns[2].update(100, false);
    btns[1].update(115, false); // btn 1 WAIT_DOUBLE
    btns[2].update(125, false); // btn 2 WAIT_DOUBLE

    // Second press on btn 2
    btns[2].update(200, true);
    btns[2].update(225, true);
    btns[2].update(270, false);
    assert(btns[2].update(295, false) == BthomeButtonEvent::DOUBLE_PRESS);
    assert(btns[2].is_idle());

    // btn 1 single press expires at 115 + 300 = 415
    assert(btns[1].update(415, false) == BthomeButtonEvent::PRESS);
    assert(btns[1].is_idle());

    // btn 0 long press fires at 25 + 700 = 725
    assert(btns[0].update(725, true) == BthomeButtonEvent::LONG_PRESS);
    btns[0].update(800, false);
    btns[0].update(825, false);
    assert(btns[0].is_idle());
    assert(btns[3].is_idle());
}

static void test_system_off_wake_short_press()
{
    // Case 1: Button released before setup() runs (latched, not currently pressed)
    ButtonStateMachine sm1;
    sm1.on_system_off_wake(0, false);
    assert(!sm1.is_idle());
    assert(sm1.update(299, false) == BthomeButtonEvent::NONE);
    assert(sm1.update(300, false) == BthomeButtonEvent::PRESS);
    assert(sm1.is_idle());

    // Case 2: Button still pressed at setup(), released 5ms later (< 25ms)
    ButtonStateMachine sm2;
    sm2.on_system_off_wake(0, true);
    assert(sm2.update(5, false) == BthomeButtonEvent::NONE);  // DEBOUNCE_RELEASE
    assert(sm2.update(30, false) == BthomeButtonEvent::NONE); // WAIT_DOUBLE
    assert(sm2.update(330, false) == BthomeButtonEvent::PRESS);
    assert(sm2.is_idle());
}

int main()
{
    test_bthome_packet_format();
    test_bthome_clamping_and_truncation();
    test_button_bounce_rejected();
    test_button_single_press();
    test_button_double_press();
    test_button_long_press();
    test_four_independent_buttons();
    test_system_off_wake_short_press();
    printf("All native tests passed!\n");
    return 0;
}

