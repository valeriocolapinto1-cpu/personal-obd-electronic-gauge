# Bill of materials — base unit

Status: phase 0 draft. Prices are indicative ranges (Nov 2026, AliExpress / Amazon IT / LCSC level)
and **must be re-checked at order time** — they were not verified live from this environment.

| # | Part | Purpose | Qty | ~€ | Notes |
|---|---|---|---|---|---|
| 1 | Waveshare ESP32-S3-Touch-LCD-2.1 | MCU, 480×480 round display, touch, buzzer, RTC, IMU, SD | 1 | 30–37 | 16 MB flash, 8 MB octal PSRAM |
| 2 | SN65HVD230 module (3.3 V) | CAN transceiver | 1 | 2 | remove the 120 Ω terminator on the module |
| 3 | L9637D (or TJA1027 / SN65HVDA195 / MC33290) + 510 Ω pull-up + ESD diode | K-line transceiver | 1 | 2–3 | |
| 4 | **SC16IS750 module, 14.7456 MHz crystal** | I2C→UART bridge for K-line + button input | 1 | 3–4 | **new (D-0.1)**, addr 0x4D |
| 5 | **ADS1115 module** | light sensor + supply voltage ADC | 1 | 2 | **new (D-0.1)**, addr 0x48 |
| 6 | Buck 12 V→5 V ≥1.5 A, low Iq | power | 1 | 2 | check quiescent current |
| 7 | Polyfuse 1 A, P-MOSFET reverse protection, TVS SMBJ24A, caps | protection | 1 set | 2 | |
| 8 | OBD-II Y splitter | coexist with Veepeak/Carista | 1 | 6–8 | |
| 9 | WS2812B ring or 144 LED/m strip | tachometer | ~30 LEDs | 3–5 | choose after measuring the board |
| 10 | LDR + resistor | light sensor (on ADS1115 AIN0) | 1 | 0.5 | |
| 11 | 12 mm momentary push button | input (on SC16IS750 GPIO0) | 1 | 1 | |
| 12 | JST-SH 4-pin socket | Qwiic port | 1 | 1 | |
| 13 | Wire, heat-shrink, protoboard | — | — | 2 | |
| 14 | Filament (ASA preferred) | enclosure | ~60 g | 2 | |
| | **Total** | | | **≈ 55–67** | over the 50 € target by ~5–15 € |
