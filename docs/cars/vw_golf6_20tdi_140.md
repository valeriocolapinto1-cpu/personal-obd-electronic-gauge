# VW Golf 6 2.0 TDI 140 hp (manual, tuned, EGR/DPF off)

| Item | Value | Status |
|---|---|---|
| Profile | `profiles/vw_golf6_20tdi_140.json` | |
| Platform | PQ35 | |
| Engine / ECU | Common rail, Bosch EDC17 (code CBAB expected) | **to confirm** |
| Protocol | CAN 500 kbit/s, 11-bit | expected, **not verified on car** |
| VIN match | prefix `WVWZZZ1KZ` | **unverified** |
| Verified on car | — | |

## Standard OBD-II PIDs used (phase 2)
| Signal | PID | Notes |
|---|---|---|
| RPM | 0x0C | |
| Coolant | 0x05 | |
| MAP / baro / boost | 0x87 or 0x0B / 0x33 / derived | tuned car: 0x0B saturates above ~1.5 bar |
| Speed | 0x0D | |
| Fuel rate | 0x5E | probably **not supported**: phase 7 computes it from injection quantity (estimated) |
| Oil temp | 0x5C | probably **not supported**: VAG measuring block in phase 7 |

The tune may change the ECU's own values: fuel consumption will be labelled "estimated".
