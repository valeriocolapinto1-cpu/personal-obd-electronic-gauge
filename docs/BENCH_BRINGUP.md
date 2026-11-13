# Phase 0 — bench bring-up

Goal: confirm the board, display, touch, buzzer, backlight and microSD work with this firmware.
**Bench only**: the board must not be connected to a car.

1. Connect the board with the Type-C port marked **USB** (native USB, not the "UART" one).
2. Build and flash (PowerShell, ESP-IDF v5.5.x environment):
   ```powershell
   cd firmware
   idf.py set-target esp32s3
   idf.py -p COMx flash monitor
   ```
   If the port is not detected: hold **BOOT**, tap **RESET**, release BOOT, then flash again.
3. Expected on screen (normal build, no car): boot sweep, splash "Universal OBD Gauge", then a gauge
   showing `--` and the bus status `bus silent` (nothing is transmitted: the CAN pins are not connected).
4. **Demo build** (animated simulated car, recommended for the bench):
   ```powershell
   idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.demo" -B build-demo -p COMx flash monitor
   ```
   - boot sweep, then "VW Scirocco 1.4 TSI 160 hp" (selected from the simulated VIN);
   - RPM screen: idle, three pulls to 6500 rpm, back to idle every 20 s;
   - swipe left/right: next/previous signal; tap: analog ↔ digital;
   - boost screen shows values above 1.5 bar (extended MAP PID simulated);
   - LVGL performance monitor (FPS/CPU) in a corner.
5. Report back: FPS shown, any flicker/tearing, touch accuracy at the edges, needle smoothness,
   measured board diameter and thickness, and GPIO0 voltage after reset (should be ~3.3 V).
