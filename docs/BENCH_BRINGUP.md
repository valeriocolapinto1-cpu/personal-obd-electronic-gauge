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
3. Expected on screen:
   - a red arc sweeping 0→100→0 with a fake RPM number (0–7000);
   - `touch x,y` updating while you drag a finger (x and y in 0..479);
   - `SD n MiB` if a FAT32 card is inserted, otherwise `SD not mounted`;
   - **BEEP** button: the buzzer sounds while pressed;
   - slider: backlight brightness 5–100 %;
   - LVGL performance monitor (FPS/CPU) in a corner.
4. Report back: FPS shown, any flicker/tearing, touch accuracy at the edges, measured board diameter
   and thickness, and GPIO0 voltage after reset (should be ~3.3 V).
