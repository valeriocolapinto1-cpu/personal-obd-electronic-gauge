# Changelog

All notable changes to this project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versioning: [SemVer](https://semver.org/).

## [Unreleased]

### Added
- Phase 3: gauge UI (LVGL v9) with an OEM-like analog dial (auto ticks/labels, smoothed needle) and a
  racing digital view (big number, progress ring, peak), display-agnostic sizes.
- Boot sequence: sweep 0 → max → 0 with ease-in-out, car name and logo placeholder.
- Navigation: swipe left/right = next/previous screen, tap = analog/digital toggle; button
  short/long press detector (for the side button).
- Themes: four built-in day/night palettes, theme JSON loader, day/night hysteresis switch.
- Unit preferences (bar relative/absolute, psi, kPa, °C/°F) applied to values and scales.
- "No data" state with the bus status; "SENSOR LIMIT" flag when PID 0x0B saturates.
- Demo mode (`sdkconfig.demo`): the whole OBD stack runs against an animated simulated car on the
  gauge itself; `sim::DemoCar` drive cycle; CI builds release and demo firmware.

### Changed
- The phase 0 bring-up screen is replaced by the gauge UI.
- Phase 2: ISO-TP (ISO 15765-2) transport with flow control, block size, STmin, WAIT/overflow, timeouts.
- OBD-II service 01/09 codec (standard PID table, support bitmaps, response validation), VIN parsing
  (check digit, model-year codes), unit conversions (boost relative/absolute, psi, °F, fuel L/100 km,
  fuel rate from injection quantity).
- `ObdSession`: functional discovery (11-bit, then 29-bit), engine-ECU choice, support bitmaps, VIN,
  cyclic polling with a global rate limit covering every request, back-off, bus-loss detection,
  derived boost, MAP saturation flag.
- Poll scheduler (fast/normal/slow, focus, back-off, not-supported disable).
- CAN bitrate detection in listen-only mode and the silent-bus policy (no transmission by default).
- Car profiles (JSON schema v1) with a built-in strict JSON parser, signal catalogue, VIN-based matching;
  profiles for the three target cars and a generic fallback.
- Firmware: TWAI driver behind `hal::ICanBus` (transmit refused in listen-only mode), bus task,
  profiles embedded at build time, live values on the bring-up screen and in the log.
- Simulator: ISO-TP-capable fake ECU (29-bit, VIN multi-frame, on/offline), end-to-end session tests.
- Docs: car sheets, `ADDING_A_CAR.md`, `PROFILE_SCHEMA.md`.
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
