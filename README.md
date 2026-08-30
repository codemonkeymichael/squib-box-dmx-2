# Squib Box DMX 2

Pico SDK C++ rewrite of the Squib-Box MicroPython controller for the original
Raspberry Pi Pico (RP2040). DMX framing is detected by a PIO state machine and
received with DMA, so channel alignment does not depend on UART polling timing.

## Behavior

- DMX input: GPIO 5 through a MAX485 receiver
- DMX channels 1-11: rising-edge 150 ms pulses
- DMX channel 12: GPIO 13 stays HIGH while active
- ON confirmation: value 110 or higher for two frames
- OFF confirmation: value 90 or lower for two frames
- Deadband 91-109 preserves the current channel state
- Pulse outputs are queued and run sequentially
- DMX activity LED: GPIO 0, dim PWM blink while frames are arriving
- Power LED: GPIO 18, dim steady PWM
- All controlled outputs start LOW
- Production firmware has USB/UART console output disabled

## Channel Map

| DMX | GPIO | Behavior |
| ---: | ---: | --- |
| 1 | 1 | 150 ms pulse |
| 2 | 2 | 150 ms pulse |
| 3 | 3 | 150 ms pulse |
| 4 | 4 | 150 ms pulse |
| 5 | 6 | 150 ms pulse |
| 6 | 7 | 150 ms pulse |
| 7 | 8 | 150 ms pulse |
| 8 | 9 | 150 ms pulse |
| 9 | 10 | 150 ms pulse |
| 10 | 11 | 150 ms pulse |
| 11 | 12 | 150 ms pulse |
| 12 | 13 | Maintained HIGH/LOW |

## Build

Run `build.cmd`, or press `Ctrl+Shift+B` in VS Code and select
`Build Squib Box DMX 2 UF2`.

The firmware is produced at:

```text
build\squib_box_dmx_2.uf2
```

Flash with all effects disconnected. Hold BOOTSEL while connecting the Pico,
then copy the UF2 to the `RPI-RP2` drive. Verify every channel mapping and power
cycle repeatedly before connecting effects.

The build uses the existing SDK at `C:\repos\Pico-Examples\pico-sdk` and the
installed ARM GNU and Visual Studio CMake/Ninja tools. It does not access or
modify BloodSquib during the build.
