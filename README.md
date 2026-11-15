# BTButton — Ultra-Low-Power 4-Button BTHome v2 BLE Device

Firmware for a CR2032-powered 4-button BLE remote implementing the **BTHome v2** specification, supporting the **PCA10040 (nRF52-DK / nRF52832)**, a **custom nRF52832 PCB**, and the **PicoMicro nRF52840**.

## Features

- **4 Independent Buttons**: Simultaneous, non-blocking per-button state machines (`ButtonStateMachine`) supporting:
  - **Single Press** (`0x01`)
  - **Double Press** (`0x02`)
  - **Long Press** (`0x04`, triggers at 700 ms hold)
  - 25 ms hardware-timer debounce, plus `NRF_P0->LATCH` wake-up seeding (`on_system_off_wake()`) so quick short presses released during bootloader startup are still captured.
- **Single Status LED**: Brief visual feedback during BLE transmission bursts (and a 150 ms pulse when entering the bootloader on PicoMicro).
- **BTHome v2 Advertising (`0xFCD2`)**:
  - Trigger-based device flag (`0x44`, v2 unencrypted)
  - Persistent 8-bit `Packet ID` (`0x00`) retained across System OFF resets (`GPREGRET` on nRF52832, `GPREGRET2` on nRF52840) for receiver deduplication
  - CR2032 `Battery %` (`0x01`) measured via `NRF_SAADC` on internal `VDD`
  - 4x `Button` objects (`0x3A`) in order
  - Complete Local Name (`BTButton`)
- **Ultra-Low-Power Architecture**:
  - **System OFF (`~0.3 µA`)**: MCU sleeps in `SYSTEMOFF` whenever all 4 buttons are idle, waking on any button press via GPIO `SENSE`.
  - **System ON (`~1.9 µA`)**: During active press/debounce/double-press windows, the CPU sleeps in `__WFE()` using `RTC2` (1024 Hz) and `GPIOTE PORT` events via `SEVONPEND` (no polling).
  - **Direct `NRF_RADIO` BLE**: Transmits `ADV_NONCONN_IND` bursts (`10` rounds × `27 ms`, ~243 ms span) on channels 37, 38, and 39 without SoftDevice overhead; HFXO and `RADIO` are powered on only during each TX burst.
  - **USB Disabled at Runtime**: On nRF52840, `NRF_USBD` and `USBD_IRQn` are disabled at startup so TinyUSB cannot preempt BLE bursts or draw extra current.
  - **DC/DC Converter**: Enabled when `BOARD_HAS_DCDC` is `1`.

## Target Differences

| Feature | `pca10040` (default) | `custom_pcb` | `picomicro_nrf52840` |
| :--- | :--- | :--- | :--- |
| **MCU** | nRF52832 | nRF52832 | nRF52840 |
| **Arduino Core** | `arduinonordicnrf5` | `arduinonordicnrf5` | `arduinoadafruitnrf52` |
| **Buttons 1–4** | `P0.13`, `P0.14`, `P0.15`, `P0.16` | Configurable in `board_config.h` | `P0.17`, `P0.20`, `P0.22`, `P0.24` |
| **Button Polarity** | Active-low (pull-up) | Configurable (`BTN_ACTIVE_LOW`) | Active-high (pull-down) |
| **Status LED** | `P0.17` (LED1), active-low | Configurable in `board_config.h` | `P0.15`, active-high |
| **TX Power** | `0 dBm` (`0x00UL`) | `0 dBm` (`0x00UL`) | `+8 dBm` (`0x08UL`, max) |
| **32.768 kHz XTAL** | `BOARD_HAS_LFXTAL = 1` | `BOARD_HAS_LFXTAL = 0` (LFRC) | `BOARD_HAS_LFXTAL = 1` |
| **Packet ID Reg** | `NRF_POWER->GPREGRET` | `NRF_POWER->GPREGRET` | `NRF_POWER->GPREGRET2` |
| **Upload Method** | J-Link (`upload_protocol = jlink`) | J-Link (`upload_protocol = jlink`) | USB DFU (`upload_protocol = nrfutil`) |
| **Bootloader Entry** | N/A | N/A | Hold **Buttons 1 + 4 for 3s** (or double-tap `RST`) |

## Custom PCB Configuration (`env:custom_pcb`)

Edit the `#if defined(BOARD_CUSTOM_PCB)` section in [include/board_config.h](include/board_config.h) to set your custom PCB's GPIO pins, active polarity, `BOARD_HAS_DCDC` (`1` if L2/L3 inductors are populated), and `BOARD_HAS_LFXTAL` (`1` if a 32.768 kHz crystal is populated, `0` to use internal LFRC).

## Build, Flash, and Test

```bash
# Build for PCA10040 (nRF52-DK)
pio run -e pca10040

# Flash to PCA10040 via onboard J-Link
pio run -e pca10040 -t upload

# Build and flash to PicoMicro nRF52840 via USB
# (If already running BTButton firmware, hold Buttons 1 + 4 for 3s or double-tap RST first to enter DFU mode)
pio run -e picomicro_nrf52840 -t upload

# Build for custom PCB
pio run -e custom_pcb

# Run native host unit tests (BTHome packet builder + button state machine)
clang++ -std=c++17 -Wall -Wextra -Iinclude test/test_native/test_main.cpp -o /tmp/btbutton_test && /tmp/btbutton_test
```
