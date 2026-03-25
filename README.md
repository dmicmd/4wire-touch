# 4-wire resistive touch library for К1986ВЕ92QI

This repository contains a compact C library for a four-wire resistive touch panel.

## Features

- touch/no-touch state;
- current normalized coordinates without panel calibration;
- accelerated deltas ready for USB HID pointer forwarding;
- single tap, double tap and tap-and-hold detection;
- tolerance to different panel resistances via polarity-compensated sampling.

## Wiring

- `Y-` -> `PD2`
- `Y+` -> `PD3`
- `X-` -> `PD6`
- `X+` -> `PD5`

## Why calibration is not required

The library returns normalized coordinates in the `0..65535` range. It reads both axes twice with opposite excitation polarity and accepts a sample only when the forward and mirrored reverse results match closely. This rejects floating-panel noise and compensates panel resistance spread, so no panel-specific resistance calibration is needed.

## Integration

1. Fill `rtouch_port_t` callbacks for GPIO direction/level switching, ADC reads, microsecond delay and millisecond timer.
2. Call `rtouch_update()` from a periodic task every 1-5 ms.
3. Read `rtouch_get_state()` and forward `dx`/`dy` to USB HID.

See `examples/mdr1986ve92qi_touch_port_example.c` for the intended wire mapping and adapter skeleton.


## PS/2 mouse bridge

The repository also contains a PS/2 mouse protocol layer in `include/ps2_mouse.h` / `src/ps2_mouse.c` and a concrete К1986ВЕ92QI example bridge in `examples/mdr1986ve92qi_touch_ps2_mouse.c`.

The PS/2 bridge uses:

- `DATA` -> `PF1`
- `CLK` -> `PF3`

The example maps touch gestures as follows:

- movement -> PS/2 `dx/dy`;
- `tap` -> left click;
- `double tap` -> double left click;
- `hold` + move -> left-button drag until finger release.


## Build firmware (.hex)

The repo now contains the SPL sources under `third_party/emdr1986x-std-per-lib` and a ready firmware entry point `firmware/main.c`.

Build with:

```bash
make
```

Expected outputs:

- `build/touchpad_ps2_mouse.elf`
- `build/touchpad_ps2_mouse.hex`
- `build/touchpad_ps2_mouse.bin`

Toolchain requirement: `arm-none-eabi-gcc` in `PATH` (or override with `TOOLCHAIN=<prefix>`).

## GitLab CI

`.gitlab-ci.yml` builds the same target and publishes `.elf/.hex/.bin/.map` artifacts.
