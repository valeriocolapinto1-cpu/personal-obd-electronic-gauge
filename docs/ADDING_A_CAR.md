# Adding a car

No change to `firmware/core/` is needed.

1. **Find the protocol.** Look at the OBD socket: pins 6 and 14 populated = CAN; pin 7 = K-line.
   The gauge also logs what it detects (`listen 500000 bit/s: N frames`).
2. **Copy a profile** from `profiles/` (start with `generic_obd.json`) to `profiles/<make>_<model>_<engine>.json`.
   Schema: [PROFILE_SCHEMA.md](PROFILE_SCHEMA.md).
3. **Match rules.** Put the WMI (VIN positions 1–3) and, if you know it, a longer VIN prefix.
   Keep `"verified": false` until checked against a real VIN. Never commit a full real VIN.
4. **Signals.** List the built-in signal ids you want, in screen order. Set alarm thresholds and the
   rev-limiter placeholder (the user sets the real value in the app).
5. **Manufacturer data** (optional): only with identifiers confirmed with a diagnostic tool, marked
   `"verified": false` until checked on the car. Never guess identifiers or scaling.
6. **Test**: `.\scripts\test.ps1` loads every profile in `profiles/` (`Profiles.AllShippedProfilesLoad`);
   add the file name to that test.
7. **Document** the car in `docs/cars/<id>.md` (protocol, ECU, PIDs, identifiers, verification date) and
   add it to the table in `README.md`.
