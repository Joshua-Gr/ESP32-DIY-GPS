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

80 × 40 mm, 4-layer PCB.

| Function | Part |
|---|---|
| MCU | Espressif ESP32-S3-WROOM-1-N16R2 (rated −40 to 85 °C ambient) |
| GNSS | u-blox NEO-M9N, external active antenna via U.FL (antenna bias from the receiver's VCC_RF) |
| Power | TI TPS54360 buck regulator, designed for 6–60 V in, 3.3 V / 1.5 A out |
| Input protection | Series Schottky (reverse polarity) and a 33 V TVS on the 12 V input |
| Connectors | 12 V input (Molex Micro-Fit 3.0), USB-C (data only, not powered), 3-pin debug UART header |

The schematic is split into sheets, each laid out in numbered blocks with design and layout notes:

| Sheet | Contents |
|---|---|
| Top level | Block diagram of how the sheets connect |
| Power | Input protection and the 3.3 V buck regulator |
| MCU | ESP32-S3 module, reset and boot buttons, decoupling, I/O series resistors |
| GNSS | NEO-M9N receiver, antenna feed, host UART and test points |
| Connectors | USB-C and the debug UART header |

## Firmware

```sh
cd firmware
pio run            # build
pio run -t upload  # flash
```

## License

Firmware: GPL-3.0-or-later. Hardware design: CERN-OHL-S-2.0. Third-party
part library: CC-BY-SA-4.0. See [LICENSE.md](LICENSE.md).
