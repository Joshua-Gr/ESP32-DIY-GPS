# Part library

Symbols, footprints and 3D models used by this board, copied from upstream
libraries so the KiCad project opens without extra libraries installed.

## License

The contents of this directory are licensed under
[CC-BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/legalcode),
with the design exception described in the upstream license files in
[`licenses/`](licenses/). That exception means the board design files
outside this directory are not bound by CC-BY-SA.

## Attribution

| File | Item | Source |
|---|---|---|
| `esp32-diy-gps.kicad_sym` | `ESP32-S3-WROOM-1` symbol | [Espressif KiCad Libraries](https://github.com/espressif/kicad-libraries) 3.2.1 |
| `esp32-diy-gps.pretty/ESP32-S3-WROOM-1.kicad_mod` | footprint | Espressif KiCad Libraries 3.2.1 |
| `3dmodels/ESP32-S3-WROOM-1.STEP` | 3D model | Espressif KiCad Libraries 3.2.1 |
| `esp32-diy-gps.kicad_sym` | `NEO-M8N`, `NEO-M9N`, `RPMH3.3-1.5` symbols | [KiCad symbols](https://gitlab.com/kicad/libraries/kicad-symbols) 10.0.7 |
| `esp32-diy-gps.pretty/ublox_NEO.kicad_mod` | footprint | [KiCad footprints](https://gitlab.com/kicad/libraries/kicad-footprints) 10.0.7 |
| `esp32-diy-gps.pretty/Converter_DCDC_RECOM_RPMx.x-x.0.kicad_mod` | footprint | KiCad footprints 10.0.7 |

`NEO-M8N` is included only because the `NEO-M9N` symbol inherits from it.

## Modifications

- Each symbol's `Footprint` field points to this project library
  (`esp32-diy-gps:…`) instead of the upstream library.
- The `ESP32-S3-WROOM-1` footprint's 3D model path points to
  `${KIPRJMOD}/lib/3dmodels/` instead of the upstream install location.

No geometry, pins or pads were changed.
