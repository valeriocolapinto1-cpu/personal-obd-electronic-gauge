# Hyundai i20 1.2 MPI (1st gen, 2008–2014)

| Item | Value | Status |
|---|---|---|
| Profile | `profiles/hyundai_i20_12_pb.json` | |
| Engine | Kappa 1.2 | |
| Protocol | **CAN or K-line?** check OBD pins 6+14 vs 7 | **open point 1** |
| VIN match | WMI `MAL` (Hyundai India) | **unverified** |
| Verified on car | — | |

Signals: coolant (0x05) and RPM (0x0C) — standard on every EOBD car. No manufacturer extension needed.
If the car is K-line only, it works from phase 6.
