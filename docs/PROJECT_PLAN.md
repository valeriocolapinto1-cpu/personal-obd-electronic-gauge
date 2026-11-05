# Universal OBD Gauge — Project Plan

Status: **phase 0 in progress** · Owner: Valerio · Licence: GPL-3.0 · Last update: 2026-11-05

This document is the single source of truth for scope, hardware, architecture and phases. Claude Code follows it together with `CLAUDE.md`. Decisions taken later are appended to §12.

---

## 1. Goal and scope

A modular, universal dashboard gauge:

- **Base unit**: reads the car's live data through the OBD-II port, **read-only**, over **CAN** or **K-line**. It auto-detects the protocol and the car (VIN) and shows **one large value per screen** on a round touch display. Around the display, an **RGB LED ring** works as a tachometer and **flashes red at the rev limiter**.
- **Modules**: optional add-ons (GPS first) plug into a **Qwiic (I2C) port**. They are detected automatically and configured from the app.
- **App**: a web page **served by the gauge** over its own Wi-Fi access point. It works on Android and iPhone with nothing to install. You use it for settings, themes, units, screen order, rev limiter, modules and **OTA updates**.
- **Universal**: car support is data (profiles) plus optional manufacturer extensions. The same firmware runs in every car.

Out of scope (for now): writing anything to ECUs, clearing DTCs, SAE J1850 (US pre-2008 GM/Ford), cloud features, CE marking (personal/portfolio project).

### Target cars

| Car | Engine | Expected protocol | Data wanted |
|---|---|---|---|
| VW Scirocco 1.4 TSI 160 hp, 2008 (PQ35) | twincharger (turbo + supercharger), Bosch MED17 — engine code to confirm (CAVD expected) | CAN 500 kbit/s, 11-bit, via gateway | Coolant temp, oil temp, intake air temp, RPM, supercharger status, **actual** boost (not requested) |
| VW Golf 6 2.0 TDI 140 hp, manual (PQ35) | common rail, Bosch EDC17 — engine code to confirm (CBAB expected). **Tuned, EGR/DPF off** | CAN 500 kbit/s, 11-bit | Boost, coolant temp, oil temp, instantaneous fuel consumption |
| Hyundai i20 1.2 MPI, 1st gen (2008–2014) | Kappa 1.2 | **To check: CAN or K-line** (pins 6+14 vs pin 7 on the OBD socket) | Coolant temp, RPM |

---

## 2. Requirements summary (from the user interview)

**Display & UI**
- One large value per screen; change screen with swipe or the physical button.
- Two gauge styles, selectable: **OEM-like analog needle** and **digital racing**.
- Themes selectable from the app. Each theme has a **darker night variant**, switched automatically by the light sensor.
- Boot: **needle sweep**, then **car name + user's custom logo**.
- Boost unit selectable from the app (bar relative / bar absolute / psi / kPa); temperatures in °C (°F optional).
- Alarms: **visual + buzzer** (e.g. coolant too hot, oil too cold to push hard, over-boost). Thresholds per car profile, editable in the app.
- Display hardware must stay **swappable** (round or rectangular, different sizes).

**LED ring (base)**
- WS2812-class RGB LEDs around the display. They fill with RPM, **flash red at the limiter**. Threshold **per car, editable in the app**.
- Brightness follows the light sensor.

**Inputs**
- Capacitive touch + **one physical push button on the side of the case**.

**Connectivity & modules**
- Qwiic/I2C port for modules, auto-detection at boot, configuration in the app.
- First module: **GPS** (real speed, 0–100 km/h timer). Antenna **inside the case** (it sits on top of the dashboard).

**Power & install**
- Powered from the OBD socket (pin 16 +12 V permanent) through an **OBD splitter (Y cable)**. Veepeak and Carista dongles stay plugged in.
- **Deep sleep** when the car is off. Must not drain the battery.
- Cables **hidden** behind the trim.
- **Thermal protection**: on top of the dashboard in summer the case can exceed 70 °C. Turn off the backlight and reduce load above a threshold.

**Enclosure**
- On top of the dashboard, same solution in all three cars.
- **As compact as possible**, **ball joint** for angle adjustment, **integrated visor** (sun glare and night reflections on the windscreen).
- Mounting with **3M VHB** tape.
- Printed by a friend/university. Material: **ASA if the printer is enclosed, PETG otherwise** (both ~20–25 €/kg; PLA is not acceptable in a car).
- Designed in **CadQuery** (parametric on display/ring diameter).

**Budget**: base unit **≤ 50 €**. Modules are budgeted separately. No deadline.

---

## 3. Hardware

### 3.1 Main board (proposed)
**Waveshare ESP32-S3-Touch-LCD-2.1** (flat or "B" curved-glass version):
- ESP32-S3R8, dual-core 240 MHz, **16 MB flash, 8 MB PSRAM**, Wi-Fi + BLE 5
- 2.1" round IPS **480×480**, ST7701 controller over **RGB parallel**, **CST820** capacitive touch (I2C)
- On board (per vendor listings — verify on the schematic): **QMI8658 IMU, PCF85063 RTC, microSD, buzzer**, battery charger. SH1.0 connectors for I2C/UART. It ships with SH1.0 cables.
- Price seen: roughly **25–32 GBP** at UK resellers, ~36 USD on marketplaces. **Expect ~30–37 € delivered.**

Why this board: it is compact, LVGL-capable with PSRAM, and already integrates touch, buzzer, RTC and SD (fewer parts). The SH1.0 4-pin I2C connector is mechanically Qwiic-compatible (check the pin order).

⚠️ **Main hardware risk — free GPIOs.** The RGB display interface uses many pins. Phase 0 must produce a verified pin map from the official schematic. Pins needed:

| Function | Pins | Notes |
|---|---|---|
| CAN (TWAI) TX/RX | 2 | any GPIO via GPIO matrix |
| K-line UART TX/RX | 2 | UART1 or UART2 |
| WS2812 data | 1 | RMT peripheral |
| Push button | 1 | wake-capable (RTC GPIO) preferred |
| Light sensor | 1 ADC | ADC1 channel (ADC2 conflicts with Wi-Fi) |
| Supply voltage sense | 1 ADC | divider from +12 V, ADC1 |
| Qwiic I2C | 0 extra | share the on-board I2C bus (touch 0x15?, IMU, RTC — check addresses vs u-blox 0x42) |

If pins are insufficient: add an I2C GPIO expander for slow signals (button), or move to a board with more free GPIO. **Ask the user before choosing.**

### 3.2 Bill of materials — base unit

Prices are indicative (Nov 2026, AliExpress/Amazon IT/LCSC-level). Phase 0 re-checks them.

| # | Part | Purpose | ~€ |
|---|---|---|---|
| 1 | Waveshare ESP32-S3-Touch-LCD-2.1 | MCU + display + touch + buzzer | 30–37 |
| 2 | SN65HVD230 CAN transceiver module (3.3 V) | CAN physical layer | 2 |
| 3 | K-line transceiver: **L9637D** (or TJA1027 / SN65HVDA195 / MC33290 — some L9637 variants are discontinued, pick what's in stock) + 510 Ω pull-up to +12 V + ESD diode | K-line physical layer | 2–3 |
| 4 | Buck converter 12 V → 5 V, ≥1.5 A, **low quiescent current** (e.g. MP1584-class module; check Iq) | power | 2 |
| 5 | Protection: 1 A polyfuse, reverse-polarity diode or P-MOSFET, **TVS** for 12 V automotive (e.g. SMBJ24A-class), bulk + ceramic caps | load dump / spikes | 2 |
| 6 | OBD-II splitter (Y) cable, 1 male → 2 female | coexist with Veepeak/Carista | 6–8 |
| 7 | WS2812B LEDs: ring **or** 144 LED/m strip bent in a circle (≈ 30 LEDs at ~70 mm Ø). Choose after measuring the board's outer diameter (ring inner Ø must clear the display) | tachometer / shift light | 3–5 |
| 8 | Light sensor: LDR + resistor (ADC), or BH1750 (I2C) if a pin is missing | auto brightness | 0.5–2 |
| 9 | 12 mm momentary push button | input | 1 |
| 10 | JST-SH 4-pin (Qwiic) socket on the case | modules port | 1 |
| 11 | Wire, heat-shrink, protoboard or small PCB (JLCPCB later, optional) | — | 2 |
| 12 | Filament for the case | enclosure | 2 |
| | **Total** | | **≈ 50–60 €** |

Budget note: the target (≤ 50 €) is reachable with the cheapest board offer and an LED strip instead of a ring. Phase 0 confirms.

### 3.3 Modules (separate budget)

| Module | Option A (plug & play) | Option B (cheap DIY) |
|---|---|---|
| GPS | SparkFun MAX-M10S Qwiic breakout (u-blox M10, I2C addr **0x42**, 10 Hz, ~46 USD + antenna) | Generic u-blox M8/M10 UART module + **SC16IS750 I2C-to-UART bridge** on a tiny board with a Qwiic socket (~12–15 €). The gauge talks I2C, the bridge talks UART to the GPS. Note: cheap NEO-6M modules are UART-only |

Future module ideas (not planned): EGT thermocouple (MAX31855 + I2C), wideband lambda, external oil pressure sensor, G-meter (the on-board IMU may already do this).

### 3.4 Wiring (base unit)

```
OBD-II plug (via splitter)
  pin 16  +12V (permanent) ──[polyfuse 1A]──[reverse prot.]──┬──[TVS to GND]
                                                            ├── Buck 12V→5V ──> board 5V in
                                                            ├── divider ──> ADC (supply sense, wake logic)
                                                            └── 510Ω pull-up ──> K-line (only via transceiver VS/K)
  pin 4/5 GND ─────────────────────────────────────────────── common GND
  pin 6   CAN-H ──┐
  pin 14  CAN-L ──┴── SN65HVD230 ── TX/RX ── ESP32 TWAI
  pin 7   K-line ──── L9637D (K) ── TX/RX ── ESP32 UART
  (pin 15 L-line: not used)

ESP32 GPIO ── WS2812 DIN (series 330Ω; check 3.3V logic vs 5V LED supply — add 74AHCT1G125 level shifter if unstable)
ESP32 ADC1 ── LDR divider
ESP32 RTC GPIO ── button to GND (internal pull-up)
I2C (on-board bus) ── Qwiic socket (3.3V, GND, SDA, SCL)
```

- CAN: do **not** add a 120 Ω termination. The car bus is already terminated.
- Phase 0 produces a proper schematic (KiCad) in `hardware/`.

### 3.5 Power management strategy
- The car is off and the bus is silent → **deep sleep**. Wake sources:
  - timer wake every N s (default 10 s) for a short check: supply voltage above ~13.0 V (alternator charging = engine running) **or** CAN traffic seen in listen-only mode;
  - push button.
- Target **sleep current < 1 mA** for the whole unit. The dev board's regulators and LEDs may draw more: **measure in phase 5**. If it is too high, add a high-side load switch so only a tiny always-on circuit stays powered. Ask the user before changing hardware.
- Brown-out handling during cranking. A clean shutdown must save the current settings.

### 3.6 Thermal
- Read the ESP32-S3 internal temperature sensor (and the RTC/IMU temperature if available).
- Above threshold T1: backlight off, LEDs off, low polling. Above T2: deep sleep until it cools down.
- The visor and a sunshade help. The case gets vents if they don't hurt the look.

---

## 4. Vehicle communication

### 4.1 Protocol auto-detection (order)
1. **CAN listen-only** at 500 kbit/s, then 250 kbit/s: look for valid frames without error counters rising. No transmission until a bitrate is confirmed.
2. Confirmed CAN → normal mode, OBD request `01 00` on 11-bit functional ID `0x7DF` (responses `0x7E8–0x7EF`). If there's no answer, try 29-bit (`0x18DB33F1`).
3. No CAN → **K-line**: ISO 14230-4 **fast init** (25 ms low / 25 ms high, StartCommunication), then ISO 9141-2 **5-baud init** (address 0x33) as fallback. Keep-alive within P3 max.
4. Read the **VIN** (service `09` PID `02`) → match a profile. Otherwise show a manual selection menu. The result is cached per VIN in NVS for a fast next boot.

### 4.2 Transport layers
- **ISO-TP (ISO 15765-2)** for multi-frame responses (VIN, VAG data): segmentation, flow control, timeouts.
- **K-line framing** (ISO 14230 header/checksum) with P1–P4 timing.
- **VAG TP2.0** (extension `ext/vag/`): channel setup on `0x200`+ECU address, parameters, keep-alive. Needed on 2008-era PQ35 engine ECUs for manufacturer data (measuring blocks via KWP2000 `0x21`). Some ECUs may also answer UDS `0x22`. **Discover per car with VCDS. Do not assume.**

### 4.3 Signal registry and polling
- Each signal: id, unit, min/max, decimals, alarm thresholds, **ordered list of sources** (e.g. `vag.mb(…)` → `obd.pid(0x0B)`), poll priority.
- Scheduler: **RPM fast** (≥ 20 Hz target on CAN, for the LED ring), the shown value fast, others slow (temperatures 1 Hz). K-line is limited to ~7–9 responses/s, so the scheduler adapts automatically.
- Derived signals: boost (rel) = MAP − barometric (PID `0x33`), fuel consumption, etc.

### 4.4 Known data notes (to verify on the cars)
- **Boost on the Scirocco/Golf**: standard PID `0x0B` (MAP) **saturates at 255 kPa absolute (~1.5 bar relative)**. The twincharger can reach that stock, and a tuned car goes higher. → Prefer the **VAG measuring block "actual charge pressure"**. Use PID `0x0B` only as a fallback with a "saturated" flag. The extended MAP PID `0x87` may or may not be supported: check the `01 80`/`01 A0` support bitmaps.
- **Oil temperature**: PID `0x5C` is often not supported on VAG of this era → VAG measuring block.
- **Supercharger (compressor) status** on the Scirocco: VAG measuring block only (clutch/bypass flap state).
- **Golf fuel consumption**: PID `0x5E` (fuel rate) is likely unsupported → compute from **injection quantity (mg/stroke) × RPM** from VAG measuring blocks, using diesel density. The tune may skew the ECU's own values: label the result "estimated".
- **i20**: only coolant temp (`0x05`) and RPM (`0x0C`) are needed. Both are standard and supported by every EOBD car, so no Hyundai extension is required for the current scope.
- Rev limiter defaults: **set by the user in the app** (the profile gives only a placeholder).

### 4.5 Coexistence with other dongles
Veepeak/Carista can send requests at the same time. Responses are matched by service echo + PID/identifier. Mismatches are dropped. VAG TP2.0 channels can collide if another tool opens one, so detect this, back off and retry later. Recommend to the user: keep the other apps closed while driving.

---

## 5. Firmware architecture

```
firmware/
  main/                 app entry, task wiring
  core/                 HW-independent C++ (host-testable)
    signals/            registry, units, conversions, derived signals
    obd/                OBD-II services 01/09 encode/decode, PID table
    transport/          ISO-TP, K-line framing state machines, allow-list (SafetyGate)
    profiles/           JSON schema, VIN parsing, matching
    scheduler/          polling priorities
    alarms/             threshold logic, hysteresis
  ext/
    vag/                TP2.0 + KWP2000 measuring blocks (+ UDS 0x22 if supported)
    hyundai/            placeholder
  hal/                  interfaces: IDisplay, ITouch, IButton, ILedRing, ILightSensor,
                        IBuzzer, ICanBus, IKLine, II2c, IStorage, IPower, IThermal
  boards/
    waveshare_s3_lcd21/ implementations + pin map
  ui/                   LVGL screens, styles (analog/digital), themes, boot sequence
  leds/                 tachometer + shift light patterns
  modules/              IModule + gps/
  net/                  softAP, HTTP server, WebSocket, OTA (dual app partitions, rollback)
  power/                sleep/wake, brown-out, thermal guard
profiles/               car profiles (JSON)
webapp/                 HTML/CSS/JS, built into LittleFS image
sim/                    host fake ECUs (CAN + K-line), replay of recorded logs
tests/                  host unit + simulator tests
hardware/               KiCad schematic, BOM
enclosure/              CadQuery sources, STL/STEP exports
docs/                   cars/, ADDING_A_CAR.md, PROJECT_PLAN.md
```

**Tasks (FreeRTOS)**: `bus` (protocol + scheduler, core 0) · `ui` (LVGL, core 1) · `leds` · `net` (on demand) · `modules`. Communication goes through a lock-free/latest-value **signal store** plus an event queue for alarms.

**Config**: NVS for settings (theme, units, screen order, limiter, alarm thresholds per profile, logo). LittleFS for the web app, the logo image and user profile overrides.

**OTA**: upload a `.bin` from the web app → `esp_ota` to the inactive partition → reboot → self-test → mark valid, or **automatic rollback**.

**Web app pages**: Live (values) · Screens (order, style per screen) · Theme (pick, night variant preview) · Car (detected profile, limiter, alarm thresholds, units) · Modules (detected, settings, GPS 0–100) · Logo upload · Update (OTA) · About/diagnostics. Wi-Fi AP: off while driving by default. It turns on with a long press of the button or for N minutes after boot (decide in phase 8).

---

## 6. UI specification

- Resolution and shape come from the board config. Layouts must scale.
- **Screen = one signal**: big number + unit, plus analog needle/arc or digital bar depending on style. Min/max peak marker optional.
- Navigation: swipe left/right or a short button press = next screen. Long press = menu / Wi-Fi AP.
- **Boot**: needle/arc sweep 0 → max → 0 (and LED ring sweep in sync), then car name + custom logo (user upload, PNG, auto-scaled), then the first screen.
- **Themes**: JSON (background, primary, accent, needle, warning, critical, font weights). Each theme defines `day` and `night` palettes. The light sensor switches between them with hysteresis.
- **Alarms**: the screen flashes the warning/critical colour, the buzzer pattern depends on severity, and the screen can auto-jump to the signal in alarm (configurable).
- "No data" state, plus a protocol/connection status icon.

## 7. LED ring specification
- N LEDs (from hardware). Fill proportional to RPM from `ring_start_rpm` to `limiter_rpm`. Colour gradient green → amber → red (theme-defined).
- At ≥ `limiter_rpm − shift_margin`: the whole ring **flashes red** (frequency configurable, default 8 Hz).
- Brightness = f(light sensor). Night cap.
- Boot sweep in sync with the needle.
- Optional: use the ring for alarms (e.g. blue pulse while oil is cold).

---

## 8. Enclosure specification (CadQuery)
- **Parameters**: display board outline (OD, thickness, connector positions), LED ring/strip diameter and width, visor depth and angle, wall thickness, ball-joint size, button position (side), Qwiic socket position, cable exit, vents.
- Parts: **front bezel** with a translucent **diffuser ring** for the LEDs (print in translucent PETG/ASA or use a laser-cut acrylic ring) · **rear shell** holding the electronics (CAN/K-line/buck on a small carrier board) · **ball joint** (sphere on the case + clamp base) · **base plate** for 3M VHB, curved to fit the dashboard · **visor** integrated into the bezel.
- GPS antenna inside the case. Keep the top side free of metal and route the antenna away from the LED wires.
- Exports: STEP + STL per part, print notes (material, orientation, supports) in `enclosure/README.md`.
- Material: ASA (enclosed printer) or PETG. Matte black.

---

## 9. Phases

Each phase = one branch + PR + green CI + a short learning summary in Italian at the end.

| # | Phase | Main deliverables | Exit criteria |
|---|---|---|---|
| 0 | **Bench bring-up & pin map** — *code done, bench test + pin map approval pending* | ESP-IDF project skeleton, board bring-up (display, touch, buzzer, SD), verified **pin map** from the schematic, BOM price check, KiCad schematic draft | LVGL demo runs on the board; pin map doc approved by the user |
| 1 | **Core + tooling** — *done* | repo structure, clang-format, host CMake, unit-test framework, **CAN/K-line simulator** skeleton, GitHub Actions CI, SafetyGate allow-list with tests | CI green; forbidden services provably blocked |
| 2 | **CAN + OBD-II** — *code merged, car test pending* | TWAI listen-only bitrate detect, ISO-TP, services 01/09, PID support bitmaps, VIN, profile matching, signal registry + scheduler | Reads RPM, coolant, IAT, MAP, baro, VIN on Scirocco and Golf (read-only); simulator tests pass |
| 3 | **UI v1** — *code done, bench/car test pending* | one-signal screens, analog + digital styles, navigation (touch + button), boot sweep + name + logo placeholder, basic themes day/night | Smooth on the bench with the simulator; tested in the car |
| 4 | **LED ring, light sensor, alarms** | WS2812 driver (RMT), tachometer + limiter flash, auto-brightness, alarm engine + buzzer | Limiter flash verified in the car; night mode works |
| 5 | **Power & robustness** | deep sleep / wake strategy, supply sense, brown-out, thermal guard, watchdog, **sleep current measurement** | Sleep current measured and documented; no drain over 72 h |
| 6 | **K-line** | L9637D (or alternative) wiring, ISO 14230 fast init + ISO 9141 5-baud, timing, adaptive scheduler | i20 shows coolant + RPM (if the i20 is K-line; otherwise tested via simulator) |
| 7 | **VAG extension** | TP2.0 + KWP measuring blocks: oil temp, actual boost, supercharger status (Scirocco), injection quantity → fuel consumption (Golf). Profile sources reordered | Values match VCDS readings ±1 unit; documented in `docs/cars/` |
| 8 | **Web app + OTA** | softAP, HTTP + WebSocket, all settings pages, logo upload, OTA with rollback | Every setting changes behaviour without reflashing; OTA + rollback tested |
| 9 | **Modules + GPS** | IModule framework, I2C probing, GPS driver (u-blox UBX over I2C), GPS speed screen, 0–100 timer | Plug the module in → it appears in the app → 0–100 works |
| 10 | **Enclosure** | CadQuery model, test prints, final print in ASA/PETG, install in the 3 cars | Fits, adjustable, readable in the sun, no rattles |
| 11 | **Portfolio polish** | README with photos/GIF/diagram, car sheets, ADDING_A_CAR guide, v1.0.0 release | Repo public-ready |

Phase 10 (CAD) can start in parallel after phase 0, once the board and LED dimensions are known.

---

## 10. Tools needed
- ESP-IDF v5.x on Windows, VS Code + ESP-IDF extension (optional), Python 3 + CadQuery.
- Soldering iron, multimeter (the user has both). A µA-capable multimeter range for the sleep current.
- **VCDS (clone)** — read-only use, to discover measuring blocks/identifiers on the two VW cars. **Do not update the clone.**
- No external CAN analyser (the user decided against it). The ESP32 itself is used as a sniffer in listen-only mode (a debug build streams frames over USB serial).

---

## 11. Open points (to resolve)
1. **i20 OBD pins**: 6+14 (CAN) or 7 (K-line)? Phase 6 depends on it, but K-line support is in scope anyway (universal gauge).
2. Engine codes: confirm CAVD (Scirocco) and CBAB (Golf) from the stickers in the service book or the engine bay.
3. Board GPIO availability (phase 0).
4. LED ring vs strip and the exact diameter (phase 0 + 10).
5. Friend's printer: enclosed (ASA) or open (PETG)?
6. ~~Unit-test framework~~ → GoogleTest (D-1.1).
7. Wi-Fi AP activation policy (phase 8).
8. GPS module option A or B (phase 9).

## 12. Decision log
- 2026-11-05 — Universal gauge: both **CAN and K-line** supported in the base unit.
- 2026-11-05 — Base = OBD read-only + display/touch + button + light sensor + buzzer + LED ring; everything else is a Qwiic module.
- 2026-11-05 — DTC clearing **excluded**. Firmware is strictly read-only.
- 2026-11-05 — Web app served by the gauge (no native app), OTA via web app.
- 2026-11-05 — ESP-IDF + C++17 moderate, LVGL v9, GPL-3.0, public GitHub repo.
- 2026-11-05 — Claude Code merges PRs autonomously only with green CI. It asks before new deps, hardware/pin changes, architecture changes and anything on the vehicle bus.
- 2026-11-05 — The user asked Claude to work **without stopping**. Decisions that CLAUDE.md says need approval are taken provisionally, marked **PROVISIONAL**, and listed in the PR for review.
- 2026-11-05 — **D-0.1 (PROVISIONAL, hardware/pin map)**: the board has only GPIO0/43/44 free. CAN TX=GPIO0, CAN RX=GPIO44, WS2812=GPIO43, console on native USB; K-line + button through an SC16IS750 I2C-UART bridge, light sensor + supply sense through an ADS1115. No button wake from deep sleep. See `docs/hardware/PIN_MAP.md`.
- 2026-11-05 — **D-0.2 (PROVISIONAL, dependency)**: only `lvgl/lvgl ~9.2` added from the component registry. Panel, touch and expander drivers written in-repo (small) instead of pulling `esp_lcd_st7701`, `esp_lcd_touch_cst816s`, `esp_io_expander_tca9554`, `esp_lvgl_port`.
- 2026-11-05 — **D-0.3**: ESP-IDF **v5.5.6** (latest v5.x). Firmware compiles with the ESP-IDF default C++ dialect; project code sticks to C++17 features. Host code builds with `-std=c++17`.
- 2026-11-05 — **D-0.4 (PROVISIONAL, open point 5)**: enclosure material **ASA** strongly preferred: PETG softens around 70–80 °C, the expected dashboard temperature.
- 2026-11-05 — **D-1.1 (open point 6)**: host unit tests use **GoogleTest** (system package on Linux CI, FetchContent on Windows). Richer assertions and parameterised tests than Unity; the firmware does not link it.
- 2026-11-05 — **D-1.2 (PROVISIONAL, safety policy detail)**: SafetyGate allows session control only with sub-function 0x01/0x03 (bit 7 ignored), so KWP `0x81` (default) passes and KWP `0x85`/`0x86`/`0x87`/`0x89` are rejected. If VAG TP2.0 needs `0x89` in phase 7, it will be asked first. The permanent deny list also covers KWP-specific write/routine/IO services (0x30, 0x32, 0x33, 0x38, 0x3D, 0x85, 0x87).
- 2026-11-05 — ESP-IDF component `hal` renamed `gauge_hal` (name clash with the ESP-IDF `hal` component).
- 2026-11-05 — **D-2.1 (PROVISIONAL, bus)**: ISO-TP frames are always sent with DLC 8 and padding byte **0x55** (ISO 15765-4 requires DLC 8; the padding value is free).
- 2026-11-05 — **D-2.2 (PROVISIONAL, bus)**: response timeout 100 ms (P2CAN 50 ms + margin), 5 s after NRC 0x78; ISO-TP N_Bs/N_Cr 1000 ms; listen window 500 ms per bitrate; every request (discovery, bitmaps, VIN, polling) shares the global ≤ 50 req/s limit; 200 ms pause of all traffic after any timeout.
- 2026-11-05 — **D-2.3 (PROVISIONAL, bus)**: silent CAN bus at both bitrates ⇒ **no transmission** (show "bus silent"). A single functional `01 00` probe at the bitrate named by the profile is only allowed with an explicit user opt-in from the app (phase 8). Needs the user's decision after checking whether the PQ35 OBD port is silent (see the first answer in chat).
- 2026-11-05 — **D-2.4**: functional discovery picks the engine ECU (0x7E8 / 0x18DAF110) when several ECUs answer; 29-bit ids are tried only if no 11-bit ECU answers.
- 2026-11-05 — **D-2.5**: VIN prefixes in the VW profiles (`WVWZZZ13Z` Scirocco, `WVWZZZ1KZ` Golf 6) and WMI `MAL` for the i20 are **unverified**; manual selection and the generic profile cover a miss.
- 2026-11-05 — **D-3.1**: navigation = swipe left/right (next/previous), tap = toggle analog/digital for the current screen, button short press = next, long press reserved for menu/Wi-Fi (phase 8).
- 2026-11-05 — **D-3.2**: the digital number uses Montserrat 48 scaled 2× (largest built-in LVGL font); a custom large font can be generated later without code changes.
- 2026-11-05 — **D-3.3**: demo build (`CONFIG_GAUGE_DEMO_MODE`) runs the real OBD stack against `sim::DemoCar` in memory; the CAN controller is never started. Used for bench and UI work (phase 3 exit criterion "smooth on the bench with the simulator").
