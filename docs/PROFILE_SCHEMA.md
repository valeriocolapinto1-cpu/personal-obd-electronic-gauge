# Car profile schema (version 1)

Profiles live in `profiles/*.json`. They are embedded in the firmware at build time and can be
overridden from the web app (phase 8). Adding a car = adding a profile (+ optional extension).

```jsonc
{
  "schema": 1,                         // required, must be 1
  "id": "vw_scirocco_14tsi_160",       // required, unique, [a-z0-9_]
  "name": "VW Scirocco 1.4 TSI 160 hp",// required, shown in the app and at boot
  "extension": "vag",                  // optional manufacturer extension ("vag", "hyundai")
  "protocol": "can_11bit_500k",        // auto | can_11bit_500k | can_29bit_500k | can_11bit_250k |
                                       // can_29bit_250k | kline_kwp_fast | kline_iso9141
  "engine": { "cylinders": 4, "fuel": "petrol" },
  "match": {
    "vin_prefixes": ["WVWZZZ13Z"],     // longest matching prefix wins (score 100 + length)
    "wmi": ["WVW"],                    // weaker match (score 10)
    "year_from": 2008, "year_to": 2017,// model-year window from VIN position 10
    "verified": false                  // true only after checking a real VIN
  },
  "rpm": { "limiter": 6500, "ring_start": 3500, "scale_max": 7000 }, // limiter: placeholder, set in the app
  "signals": [
    {
      "id": "coolant_temp",            // built-in id inherits label/unit/range/sources
      "label": "COOLANT", "min": -40, "max": 140, "decimals": 0,
      "priority": "slow",              // fast | normal | slow
      "sources": [                     // tried in order; first available wins
        { "manufacturer": "vag:...", "verified": false },
        { "obd_pid": 5 }
      ],
      "alarm": { "warn_low": 60, "warn_high": 105, "crit_high": 112, "hysteresis": 1 }
    }
  ]
}
```

Units are internal: °C, kPa (absolute for `intake_map`/`barometric`, relative for `boost`), rpm, km/h, L/h.
Display units (°F, bar, psi) are user settings.

Built-in signal ids: `engine_rpm`, `coolant_temp`, `oil_temp`, `intake_air_temp`, `intake_map`,
`barometric`, `boost`, `vehicle_speed`, `engine_load`, `throttle`, `maf_rate`, `module_voltage`,
`ambient_temp`, `fuel_rate`.

**Rule**: never invent manufacturer identifiers or scaling. Anything not confirmed on a car is
`"verified": false` and listed in the car sheet in `docs/cars/` for checking with VCDS.
