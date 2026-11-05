# Pin map — Waveshare ESP32-S3-Touch-LCD-2.1

Status: **PROVISIONAL — waiting for user approval** (CLAUDE.md: pin map changes need the user's OK).
Source: official schematic `ESP32-S3-Touch-LCD-2.1_schematic_diagram.pdf` (Waveshare wiki) and the
vendor ESP-IDF demo `ESP32-S3-Touch-LCD-2.1-Test`. Code: `firmware/boards/waveshare_s3_lcd21/board/include/board/board_pins.h`
(the only file with GPIO numbers).

## 1. What the board already uses

| GPIO | Function | Notes |
|---|---|---|
| 1, 2 | ST7701 3-wire SPI (SDA, SCL) **and** microSD SDMMC CMD, CLK | shared: LCD init first, then SD |
| 3, 5, 8, 9–14, 17, 18, 21, 45–48 | RGB565 data bus | B0 and R0 not connected |
| 38, 39, 40, 41 | HSYNC, VSYNC, DE, PCLK | |
| 6 | Backlight PWM (via SI2302 MOSFET) | |
| 7, 15 | I2C SCL, SDA | touch, IMU, RTC, expander, **Qwiic port** |
| 16 | Touch INT | |
| 42 | microSD D0 | 1-bit SDMMC |
| 4 | Li-ion battery ADC (200k/100k divider) | on-board battery only |
| 19, 20 | Native USB D-/D+ | Type-C "USB" port, console + flashing |
| 26–32 | SPI flash | internal |
| 33–37 | Octal PSRAM | internal ("ESP32-S3R8") |
| 43, 44 | UART0 TX/RX | routed by an FSUSB42 switch to the CH343 Type-C port **or**, when that port is unplugged, to the UART connector and the 12-pin header |
| 0 | BOOT key (10k pull-up), on the 12-pin header as "GPIO0" | strapping pin |

TCA9554 expander (I2C 0x20): EXIO1 LCD reset · EXIO2 touch reset · EXIO3 LCD CS · EXIO4 SD D3 ·
EXIO5/6 IMU INT2/INT1 · EXIO7 RTC INT · EXIO8 **buzzer** (high = on). All 8 bits are used.

I2C addresses in use: 0x15 CST820 touch · 0x20 TCA9554 · 0x51 PCF85063 RTC · 0x6B QMI8658 IMU ·
0x7E (listed by Waveshare, device not identified).

## 2. Free pins — the main hardware risk is confirmed

Only **GPIO0, GPIO43, GPIO44** (and 19/20 if native USB is given up) are available.
The plan needs 8: CAN TX/RX, K-line TX/RX, WS2812, button, light sensor ADC, supply ADC.
None of the free pins is on ADC1, so no direct analog input is possible.

## 3. Proposed allocation (decision D-0.1, provisional)

| Function | Where | Why |
|---|---|---|
| Console + flashing | native USB-Serial-JTAG, GPIO19/20 (Type-C "USB" port) | frees UART0 pins |
| **CAN TX** | **GPIO0** (12-pin header pin 12) | the ROM never drives GPIO0 and its pull-up keeps the transceiver **recessive** during reset/boot. Putting CAN TX on GPIO43 would put the ROM boot log on the car bus (error frames) — not acceptable (CLAUDE.md safety rule 2) |
| **CAN RX** | **GPIO44** (UART connector RXD) | input during boot, no strapping function |
| **WS2812 data** | **GPIO43** (UART connector TXD, 499 Ω on board) | the ROM boot log may flash a few LEDs for ~0.1 s after reset; the driver clears the ring at start-up. Can be removed by burning the `UART_PRINT_CONTROL` eFuse (irreversible — only if the user wants) |
| **K-line** | **SC16IS750** I2C→UART bridge on the I2C bus (addr **0x4D**, A0=A1=GND) + L9637D | fast init (25 ms low) via the bridge's break bit; 5-baud init by toggling break with 200 ms bit timing; 10400 baud with a 14.7456 MHz crystal (−0.45 %) |
| **Push button** | SC16IS750 GPIO0 (input, pull-up) | polled at 50 Hz |
| **Light sensor + supply sense** | **ADS1115** 4-ch ADC on I2C (addr **0x48**) | AIN0 LDR divider, AIN1 +12 V divider, AIN2/3 spare (case NTC) |

Rules kept: the **BOOT key must not be pressed while in the car** (it would short CAN TX to GND,
dominant bit). It sits inside the case, so this is a bench-only concern. The CH343 Type-C port must stay
unplugged in the car (it reroutes GPIO43/44 to the USB bridge); use the native "USB" port for the bench.

### Consequences
- Button **cannot wake** the ESP32 from deep sleep (no RTC GPIO left). Wake = timer every N s
  (supply voltage check), as already planned in §3.5. Max wake latency = N s.
- BOM grows by ~4–6 € (SC16IS750 module with 14.7456 MHz crystal, ADS1115 module). Base unit ≈ 55–65 €.

## 4. Alternatives the user can choose instead

| Option | Pros | Cons |
|---|---|---|
| **A (proposed)** I2C bridge + I2C ADC on this board | keeps the compact round board; one firmware | K-line timing through I2C; no button wake; +5 € |
| B Co-processor (e.g. ESP32-C3 mini, ~3 €) on I2C handling CAN + K-line + button + ADC | real-time bus handling, button wake via its own GPIO | two firmwares, more complexity |
| C Different board with free GPIOs (SPI display instead of RGB) | everything direct | redesign of display/enclosure, slower display refresh |

## 5. Bench verification checklist (user)
1. Flash via the native USB port; check the bring-up screen (arc sweep, touch coordinates, BEEP, backlight slider, SD size).
2. Measure GPIO0 with a multimeter after reset: must read 3.3 V (recessive for CAN TX).
3. Confirm the outer diameter and thickness of the board with calipers (for the LED ring and the enclosure).
