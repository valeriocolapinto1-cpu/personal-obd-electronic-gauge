# CLAUDE.md — Universal OBD Gauge

Rules for Claude Code working on this repository. Read this file and `docs/PROJECT_PLAN.md` before starting any task.

## Project in one paragraph
A modular, universal digital gauge for cars. The **base unit** reads live data from the OBD-II port (read-only) over **CAN (ISO 15765-4)** or **K-line (ISO 9141-2 / ISO 14230-4)**, shows one large value per screen on a round touch display, and drives an **RGB LED ring** used as a tachometer and shift light. Optional **modules** (GPS first) plug into a Qwiic/I2C port, are auto-detected and configured from a **web app served by the gauge itself** over Wi-Fi. Target cars: VW Scirocco 1.4 TSI 160 hp (2008), VW Golf 6 2.0 TDI 140 hp (manual, tuned, EGR/DPF off), Hyundai i20 1.2 MPI (1st gen, 2008–2014). Any other car must be addable by writing a profile, without touching core code.

## Language and stack
- **Everything in English**: code, comments, commit messages, docs, issue/PR text.
- Firmware: **ESP-IDF** (latest stable v5.x), **C++17, moderate style**: classes for drivers, profiles and modules; no exceptions, no RTTI, no heavy template metaprogramming, no Boost. Plain C APIs from ESP-IDF are fine.
- UI: **LVGL v9** (via the ESP-IDF component registry).
- Web app: plain HTML/CSS/JS (no framework unless approved), stored on LittleFS, served by `esp_http_server` with WebSocket for live data.
- Enclosure: **CadQuery** (Python), fully parametric.
- Host tools/tests: CMake + a host build of the platform-independent code (no ESP-IDF headers in `core/`).
- Development machine: **Windows** (native ESP-IDF install). Scripts must work in PowerShell; avoid bash-only tooling in the main workflow.
- Formatting: **clang-format** using the ESP-IDF style (`.clang-format` in repo root). CI fails on unformatted code.

## How to work
- Work **autonomously**. Do not narrate each step or explain code while working.
- Work **one phase at a time** as defined in `docs/PROJECT_PLAN.md`, one branch per phase (`phase-N-short-name`).
- **Stop and ask the user before**:
  1. adding any new library, component or dependency;
  2. changing hardware, wiring or the pin map;
  3. changing the architecture (module boundaries, interfaces in `core/`, data flow);
  4. **anything that touches what is transmitted on the vehicle bus** (CAN or K-line): new services, new request IDs, timing, init sequences, tester addresses.
  When asking, give the options, a recommendation and the trade-off in a few lines.
- **At the end of every phase**, write a **short learning summary** for the user (in Italian, max ~15 lines) in the PR description and in the chat: what was built, why it was done this way, and the 1–3 key concepts behind it (e.g. "what ISO-TP flow control is"). No explanations during the work, only at the end.

## Git
- **Conventional Commits** (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`, `ci:`), scope optional (`feat(can): ...`).
- Branch per phase → Pull Request → **Claude may merge by itself only if CI is fully green** (build + format + unit tests + simulator tests). Never merge with failing or skipped tests. Never force-push `main`.
- Update `CHANGELOG.md` (Keep a Changelog format) in every PR.
- Licence: **GPL-3.0**. Every source file starts with an SPDX header: `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Vehicle bus safety — NON-NEGOTIABLE
The gauge is a **read-only** device. Breaking these rules can disable a car while driving.
1. **Allow-list at the lowest transmit layer.** Every outgoing diagnostic request passes through one function that checks the service ID against a compile-time allow-list. Anything not listed is dropped and logged. Allowed:
   - OBD-II: service `0x01` (current data), `0x09` (vehicle info: VIN, CALID), `0x03`/`0x07` only if a later phase explicitly enables DTC *reading*.
   - Manufacturer (only inside manufacturer extensions): `0x10` with **default or extended diagnostic session only** (never programming), `0x1A`/`0x22` read identifiers, `0x21` read local identifier (KWP measuring blocks), `0x3E` tester present, and the VAG TP2.0 channel setup/keep-alive frames.
   - **Explicitly forbidden, forever**: clearing DTCs (`0x04`, `0x14`), security access (`0x27`), any write (`0x2E`, `0x3B`, `0x2F`), routines (`0x31`), ECU reset (`0x11`), communication control (`0x28`), download/upload (`0x34`–`0x37`), programming sessions.
   - Unit tests must prove that every forbidden service is rejected.
2. **Never transmit before the bus is identified.** CAN bitrate detection runs in **TWAI listen-only mode**. A transmit with the wrong bitrate creates error frames on a live bus.
3. **Rate limits**: max request rate per ECU defined in the profile; default total ≤ 50 requests/s on CAN, protocol-mandated timings on K-line (P1–P4). Back off immediately on timeouts or negative responses.
4. **Coexistence**: other OBD dongles (Veepeak, Carista) may stay plugged in through a splitter. Validate every response (service echo + PID/identifier match). Discard unsolicited or mismatched frames and do not crash or hang.
5. The firmware **never** writes coding, adaptations or anything persistent to any ECU.
6. Flashing the ESP32 is allowed **only on the bench**, never while the gauge is connected to a car. Before flashing, Claude asks the user to confirm the device is on the bench.

## Architecture rules
- `core/` is **hardware-independent C++** (signal registry, decoders, unit conversions, profile matching, scheduler logic, protocol state machines). It must compile and run on the host for tests.
- Hardware lives behind interfaces in `hal/` (display, touch, button, LED ring, light sensor, buzzer, CAN, K-line UART, I2C, storage, power). Each board gets its own `boards/<board-name>/` implementation with a pin map header — **no GPIO numbers anywhere else**.
- **Display-agnostic UI**: screens get the resolution and shape (round/rect) from the board config; nothing hard-codes 480×480.
- **Car profiles** are data (JSON in `profiles/`, embedded at build time and overridable from the web app). They declare: matching rules (VIN prefix/WMI, model year range, manual name), protocol hints, signals with source priority (e.g. VAG measuring block first, standard PID fallback), scaling, ranges, alarm thresholds, rev limiter default.
- **Manufacturer extensions** (`ext/vag/`, `ext/hyundai/`) implement extra transports/services and are selected by the profile. Adding a new car = new profile JSON (+ optional extension) + docs page. No changes in `core/`.
- **Modules** (`modules/`) implement one `Module` interface (probe → init → poll → publish signals → config schema for the web app). Detected by I2C probing at boot. A broken or missing module must never block the base gauge.
- A **hardware watchdog** stays enabled. The UI must keep running (showing "no data") if the bus disappears.
- Performance: no strict RAM/CPU budget for now — correctness and clarity first, optimise later. Keep PSRAM for LVGL buffers.

## Testing (required for merge)
- **Unit tests** (host, GoogleTest or Unity — ask once, then keep it): every decoder, scaling formula and unit conversion (PIDs, measuring blocks, boost relative/absolute, °C, bar/psi, fuel consumption), the allow-list, ISO-TP segmentation/reassembly, VIN parsing and profile matching.
- **CAN simulator**: a host-side fake ECU (and a K-line fake ECU) that answers OBD/VAG requests from recorded or scripted data, including timeouts, negative responses and foreign-dongle traffic. Used by tests and runnable on the PC for UI development.
- **CI (GitHub Actions)**: ESP-IDF build for every board, clang-format check, host unit tests, simulator tests. Runs on every push and PR.

## Documentation to keep up to date (in every relevant PR)
- `README.md` — portfolio-quality: photos, features, wiring diagram, build/flash instructions, supported cars table.
- `docs/cars/<car>.md` — one sheet per car: protocol, ECU, supported PIDs, manufacturer identifiers/measuring blocks used, scaling, verified-on-car date.
- `docs/ADDING_A_CAR.md` — step-by-step guide to add a new vehicle.
- `CHANGELOG.md`.
- `docs/PROJECT_PLAN.md` — mark phases done; record decisions taken after asking the user.

## Things not to do
- Do not invent PIDs, measuring block numbers or scaling. If a value is not documented or verified, mark it `unverified` in the profile and ask the user to check it with VCDS or on the car.
- Do not add cloud services, accounts or telemetry. The gauge works fully offline.
- Do not commit secrets, Wi-Fi passwords or real VINs (use placeholders in tests and docs).
