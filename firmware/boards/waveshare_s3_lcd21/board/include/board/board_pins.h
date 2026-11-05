// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pin map for the Waveshare ESP32-S3-Touch-LCD-2.1.
//
// This is the ONLY file in the repository that contains GPIO numbers for this board.
// Source: official schematic "ESP32-S3-Touch-LCD-2.1_schematic_diagram.pdf" and the
// vendor ESP-IDF demo. Rationale and status in docs/hardware/PIN_MAP.md.
#pragma once

#include <cstdint>

namespace board::pins {

// ---------------------------------------------------------------------------
// Display: ST7701 over 16-bit RGB565 parallel + 3-wire SPI for the init sequence
// ---------------------------------------------------------------------------
constexpr int kLcdPclk = 41;
constexpr int kLcdDe = 40;
constexpr int kLcdVsync = 39;
constexpr int kLcdHsync = 38;
// RGB565 data bus, index 0 = B LSB ... 15 = R MSB.
constexpr int kLcdData[16] = {
    5,  45, 48, 47, 21,    // B1..B5 (B0 not connected)
    14, 13, 12, 11, 10, 9, // G0..G5
    46, 3,  8,  18, 17,    // R1..R5 (R0 not connected)
};
constexpr int kLcdBacklight = 6; // LEDC PWM, high = on
// 3-wire SPI used only during panel init. Shared with the microSD card (SDMMC CMD/CLK).
constexpr int kLcdSpiSda = 1;
constexpr int kLcdSpiScl = 2;

constexpr uint16_t kLcdWidth = 480;
constexpr uint16_t kLcdHeight = 480;
constexpr uint32_t kLcdPclkHz = 18 * 1000 * 1000;

// ---------------------------------------------------------------------------
// On-board I2C bus (also routed to the SH1.0 I2C connector = Qwiic port)
// ---------------------------------------------------------------------------
constexpr int kI2cSda = 15;
constexpr int kI2cScl = 7;
constexpr uint32_t kI2cHz = 400 * 1000;

// I2C 7-bit addresses already used on the board.
constexpr uint8_t kAddrTouchCst820 = 0x15;
constexpr uint8_t kAddrExpanderTca9554 = 0x20;
constexpr uint8_t kAddrRtcPcf85063 = 0x51;
constexpr uint8_t kAddrImuQmi8658 = 0x6B;

constexpr int kTouchInt = 16;

// ---------------------------------------------------------------------------
// TCA9554 IO expander bits (EXIO1..EXIO8 -> bit 0..7)
// ---------------------------------------------------------------------------
constexpr uint8_t kExioLcdReset = 0;   // EXIO1
constexpr uint8_t kExioTouchReset = 1; // EXIO2
constexpr uint8_t kExioLcdCs = 2;      // EXIO3
constexpr uint8_t kExioSdCs = 3;       // EXIO4 (SD D3)
constexpr uint8_t kExioImuInt2 = 4;    // EXIO5 (input)
constexpr uint8_t kExioImuInt1 = 5;    // EXIO6 (input)
constexpr uint8_t kExioRtcInt = 6;     // EXIO7 (input)
constexpr uint8_t kExioBuzzer = 7;     // EXIO8, high = on

// ---------------------------------------------------------------------------
// microSD, SDMMC 1-bit (CLK/CMD shared with the LCD init SPI)
// ---------------------------------------------------------------------------
constexpr int kSdClk = 2;
constexpr int kSdCmd = 1;
constexpr int kSdD0 = 42;

// ---------------------------------------------------------------------------
// Battery sense (on-board Li-ion divider 200k/100k). Not the car supply.
// ---------------------------------------------------------------------------
constexpr int kBatAdc = 4;

// ---------------------------------------------------------------------------
// Gauge I/O on the free pins. PROVISIONAL, pending user approval
// (see docs/hardware/PIN_MAP.md, "Free pins" and decision D-0.1).
// ---------------------------------------------------------------------------
// CAN (TWAI) via SN65HVD230. TX on GPIO0: the ROM never drives it and its
// pull-up keeps the transceiver recessive during reset and boot.
constexpr int kCanTx = 0;  // SH1.0 12-pin header, pin 12 (also BOOT key)
constexpr int kCanRx = 44; // UART connector RXD (U0RXD)
// WS2812 data. ROM boot messages appear on this pin for ~100 ms after reset;
// the LED driver clears the ring as soon as it starts.
constexpr int kLedRingData = 43; // UART connector TXD (U0TXD, 499R series)

// Native USB-Serial-JTAG (console + flashing). Reserved.
constexpr int kUsbDm = 19;
constexpr int kUsbDp = 20;

} // namespace board::pins
