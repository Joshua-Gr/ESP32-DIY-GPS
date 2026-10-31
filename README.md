# ESP32 DIY GPS

A GPS receiver board built around an ESP32-S3 and a u-blox GNSS module, powered from a 12 V vehicle supply.

## Repository layout

| Path | Contents |
|---|---|
| `firmware/` | PlatformIO project (Arduino framework) |
| `hardware/` | KiCad 10 project |
| `hardware/lib/` | Project-local symbols, footprints and 3D models, so the project opens without extra libraries installed |
| `hardware/fab/` | Fabrication outputs, one folder per ordered board revision |

## Hardware

| Function | Part |
|---|---|
| MCU | Espressif ESP32-S3-WROOM-1-N16R8 |
| GNSS | u-blox NEO-M9N, external active antenna via SMA |
| Power | RECOM RPMH3.3-1.5, 5–60 V in, 3.3 V out |

## Firmware

```sh
cd firmware
pio run            # build
pio run -t upload  # flash
```

## License

Firmware: GPL-3.0-or-later. Hardware design: CERN-OHL-S-2.0. Third-party
part library: CC-BY-SA-4.0. See [LICENSE.md](LICENSE.md).
