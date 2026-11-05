# Changelog

All notable changes to this project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning: [SemVer](https://semver.org/).

## [Unreleased]

### Added
- Phase 1: hardware-independent `core` library (ESP-IDF component and host library).
- **SafetyGate**: compile-time allow-list for every diagnostic request (default deny, permanent deny list,
  OBD/manufacturer contexts, session check) and `GatedRequestSink`, the only path to a bus transport.
- Unit tests proving every forbidden service is rejected (exhaustive over all 256 service IDs).
- CAN simulator skeleton: virtual bus with simulated clock, scripted fake OBD ECU (timeouts, negative
  responses, support bitmaps), foreign-dongle traffic generator.
- Host build (CMake, C++17, GoogleTest), ASan/UBSan, CI jobs for Linux and Windows, `scripts/test.ps1`.
- Phase 0: ESP-IDF v5.5 project skeleton for the Waveshare ESP32-S3-Touch-LCD-2.1.
- Board support: TCA9554 expander, ST7701 RGB panel (double frame buffer in PSRAM, VSYNC swap),
  CST820 touch, backlight PWM, buzzer, microSD (SDMMC 1-bit).
- HAL interfaces: display, touch, buzzer, removable storage.
- LVGL v9 glue and a bench bring-up screen.
- Verified pin map from the official schematic (`docs/hardware/PIN_MAP.md`), provisional allocation of the
  free pins (decision D-0.1), BOM draft.
- CI: clang-format check and ESP-IDF build; cross-platform format script.
