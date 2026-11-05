# Testing

## Host tests (Windows / Linux)
```powershell
.\scripts\test.ps1          # configure, build, run unit + simulator tests
```
Linux: `cmake -S . -B build -DGAUGE_SANITIZE=ON && cmake --build build && ctest --test-dir build`.

- `tests/unit/` — pure unit tests of `firmware/core` (labels `unit`).
- `tests/sim/` — tests running against the CAN simulator in `sim/` (label `sim`).

## Simulator
`sim/` contains a virtual CAN bus with a simulated millisecond clock (`VirtualCanBus`), a scripted
fake OBD ECU (`FakeObdEcu`: per-PID data, delay, timeout, negative response, automatic support bitmaps)
and a `ForeignDongle` that polls PIDs like a Veepeak/Carista sharing the bus.

## Safety tests
`tests/unit/test_safety_gate.cpp` proves that every service CLAUDE.md lists as forbidden is rejected in
every context with every sub-function byte, and that the allow-list is exactly the documented one
(exhaustive over all 256 service IDs).
