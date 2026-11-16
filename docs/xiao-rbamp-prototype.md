# Quintinity prototype: XIAO ESP32-C3 + rbAmp UI1

## Status

**Working and tested on real hardware.** Bradley confirmed on 2026-11-15 that the
assembled prototype works on the bench.

Still to be added to this repository:

- [x] Firmware / configuration used for the test (from Minjae):
      [prototype/xiao-rbamp/](../prototype/xiao-rbamp/)
- [x] Logs and readings: [xiao-rbamp-test-log.md](xiao-rbamp-test-log.md)
- [ ] Photos of the setup
- [ ] Voltage, power and energy readings (needs the L/N mains connection) compared
      against a reference meter or known load

Minjae's bench tests (12–13 Nov) confirmed the I2C link, current readings and the live
web page. Voltage, power and energy were **not** tested, because L/N was not connected,
and the current scale is not yet verified. See the test log for details.

The firmware under `esphome-esp32emon/` is the upstream analog design and does **not**
match the tested device (see "How this differs from the original design" below).

## Hardware

| Part | Notes |
| --- | --- |
| Seeed XIAO ESP32-C3 | With the XIAO FPC antenna on the U.FL connector (the C3 has no on-board antenna). |
| rbAmp UI1 Basic I2C energy module | Built-in voltage transformer, L/N screw terminal, 3.5 mm CT jack. I2C address `0x50`. |
| YHDC SCT013 5A/1V current clamp | Voltage-output clamp with built-in burden; plugs straight into the UI1. |

### Wiring

| rbAmp UI1 | XIAO ESP32-C3 |
| --- | --- |
| Vin | 5V (the module needs 4.5–5.5 V, not 3V3) |
| GND | GND |
| SDA | D4 (GPIO6) |
| SCL | D5 (GPIO7) |
| DRDY | not connected |

The UI1 has on-board 4.7 kΩ pull-ups to 3.3 V. Any extra pull-ups must go to 3V3,
never 5V, because the C3's pins are not 5 V tolerant. Run I2C at 50 kHz.

### Mains safety

rbAmp requires a fuse of 0.1 A or less in the L line. Wire L to L and N to N.
Keep the board in an enclosure whenever mains is connected.

## How this differs from the original design

The rest of this repository is a fork of
[danpeig/ESP32EnergyMonitor](https://github.com/danpeig/ESP32EnergyMonitor), which
uses an ESP32 DevKit, a ZMPT101B analog voltage sensor and EmonLib. That firmware
reads analog pins and does not work with the I2C rbAmp module. The prototype
instead uses the rbAmp ESPHome component
([rb-amp/rbamp-esphome](https://github.com/rb-amp/rbamp-esphome)) with
`board: seeed_xiao_esp32c3`, `sda: GPIO6`, `scl: GPIO7` and `address: 0x50`. The tested
config ran the bus at `frequency: 10kHz` (50 kHz is the rbAmp recommendation) and had
`ct_model` commented out, because this production module refused the CT-model write.

Known issue to check against when validating readings:
[rbamp-esphome#1](https://github.com/rb-amp/rbamp-esphome/issues/1) (UI1 Basic
reads current 3× too high if the CT model setting does not take effect).
