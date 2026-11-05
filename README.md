# Universal OBD Gauge

A modular, **read-only** digital gauge for cars: one large value per screen on a round 2.1" touch display,
an RGB LED ring as tachometer and shift light, live data from the OBD-II port over **CAN** or **K-line**,
and optional plug-in modules (GPS first) configured from a web app served by the gauge itself.

> Status: **early development** — see [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md) for phases.

## Features (planned)
- Protocol and car auto-detection (CAN 500/250 kbit/s, ISO 9141-2, ISO 14230-4), VIN-based car profiles
- Strict read-only safety gate: only OBD services 01/09 (plus whitelisted manufacturer *read* services)
- Analog-needle and digital-racing styles, day/night themes with automatic switching
- LED ring tachometer with per-car limiter flash
- Alarms (visual + buzzer), Wi-Fi web app for settings and OTA updates
- Qwiic/I2C module port (GPS: speed, 0–100 km/h timer)

## Supported cars

| Car | Protocol | Status |
|---|---|---|
| VW Scirocco 1.4 TSI 160 hp (2008) | CAN 500k | standard PIDs in simulator, car test pending ([sheet](docs/cars/vw_scirocco_14tsi_160.md)) |
| VW Golf 6 2.0 TDI 140 hp | CAN 500k | standard PIDs in simulator, car test pending ([sheet](docs/cars/vw_golf6_20tdi_140.md)) |
| Hyundai i20 1.2 MPI (1st gen) | CAN or K-line (to check) | CAN in simulator ([sheet](docs/cars/hyundai_i20_12_pb.md)) |
| Any other OBD-II car | CAN | generic profile; see [ADDING_A_CAR](docs/ADDING_A_CAR.md) |

## Hardware
Base board: Waveshare ESP32-S3-Touch-LCD-2.1. Pin map: [docs/hardware/PIN_MAP.md](docs/hardware/PIN_MAP.md) ·
BOM: [docs/hardware/BOM.md](docs/hardware/BOM.md).

## Build and flash (Windows, PowerShell)
Requires ESP-IDF v5.5.x installed natively. **Flash only on the bench, never while connected to a car.**

```powershell
cd firmware
idf.py set-target esp32s3      # first time only
idf.py build
idf.py -p COMx flash monitor   # use the Type-C port marked "USB" (native USB)
```

See [docs/BENCH_BRINGUP.md](docs/BENCH_BRINGUP.md) for the phase 0 bench test.

## Repository layout
```
firmware/   ESP-IDF project (main, hal, boards/<board>, later core/, ext/, ui/, ...)
docs/       project plan, hardware notes, car sheets
scripts/    formatting and helper scripts (PowerShell + Python)
```

## Licence
GPL-3.0-or-later. See [LICENSE](LICENSE).
