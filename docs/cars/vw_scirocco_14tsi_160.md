# VW Scirocco 1.4 TSI 160 hp (2008)

| Item | Value | Status |
|---|---|---|
| Profile | `profiles/vw_scirocco_14tsi_160.json` | |
| Platform | PQ35 | |
| Engine / ECU | Twincharger, Bosch MED17 (code CAVD expected) | **to confirm** (engine sticker) |
| Protocol | CAN 500 kbit/s, 11-bit, via gateway | expected, **not verified on car** |
| VIN match | prefix `WVWZZZ13Z` (positions 7–8 = type "13") | **unverified** |
| Verified on car | — | |

## Standard OBD-II PIDs used (phase 2)
| Signal | PID | Formula | Notes |
|---|---|---|---|
| RPM | 0x0C | (256A+B)/4 | |
| Coolant | 0x05 | A−40 °C | |
| Intake air temp | 0x0F | A−40 °C | |
| MAP | 0x87 if supported, else 0x0B | (256B+C)/32 kPa / A kPa | **0x0B saturates at 255 kPa** (~1.5 bar boost): flagged "saturated" |
| Barometric | 0x33 | A kPa | |
| Boost | derived | MAP − baro | relative |
| Oil temp | 0x5C | A−40 °C | probably **not supported** on this ECU: VAG measuring block in phase 7 |
| Module voltage | 0x42 | (256A+B)/1000 V | |

## Manufacturer data (phase 7, to discover with VCDS)
Actual boost, oil temperature, supercharger (compressor clutch/bypass) status: VAG measuring blocks,
**numbers not known yet** — to be read with VCDS (read-only) and recorded here.

## To check on the car (user)
1. With the gauge on the bench build: does the OBD port carry CAN traffic with the engine running and no tester connected? (the gauge logs "listen 500000 bit/s: N frames")
2. Engine code from the sticker.
3. Real VIN positions 1–9 (do not publish the full VIN).
