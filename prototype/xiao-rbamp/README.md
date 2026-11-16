# XIAO ESP32-C3 + rbAmp UI1 firmware (as tested)

ESPHome firmware used for the Quintinity prototype bench tests on 12–13 Nov 2026.
See [docs/xiao-rbamp-prototype.md](../../docs/xiao-rbamp-prototype.md) for the hardware
and wiring, and [docs/xiao-rbamp-test-log.md](../../docs/xiao-rbamp-test-log.md) for the
test results.

| File | Purpose |
| --- | --- |
| `rbamp-meter.yaml` | The meter firmware: rbAmp component, web page on port 80, 5 s updates. |
| `live-chart.js` | Adds a pulsing "live" dot to the web page charts (loaded via `js_include`). |
| `EXAMPLE_secrets.yaml` | Template for `secrets.yaml` (Wi-Fi, API key, OTA password). |
| `diag/` | Diagnostic firmware written while debugging the I2C link (see below). |

## Build and flash

```bash
cp EXAMPLE_secrets.yaml secrets.yaml   # then edit it
esphome run rbamp-meter.yaml --device /dev/ttyACM0
```

ESPHome 2026.10.2, ESP-IDF framework. The rbAmp component is pulled from
`github://rb-amp/rbamp-esphome` (unpinned; tested against commit `5fd6ad6` of `main`).

## Settings that differ from the rbAmp docs

- **`frequency: 10kHz`** instead of the recommended 50 kHz. This was lowered while
  debugging and was never moved back to 50 kHz after the wiring fix.
- **`ct_model:` is commented out.** This production module refused the CT-model write
  (the component says this needs factory mode). The module uses its stored CT model, so
  the current scale is **not yet verified** against a reference. See
  [rbamp-esphome#1](https://github.com/rb-amp/rbamp-esphome/issues/1) (current reads 3× high
  when the CT model setting does not take effect).
- **Self-heal `interval:`** reboots the ESP if the rbAmp did not answer at boot, because
  the component only probes once in `setup()`.
- **`api: reboot_timeout: 0s`**, because there is no Home Assistant. Without it, the ESP reboots every 15 min.
- **`wifi: power_save_mode: none`**, because modem sleep made the C3 miss ARP and become unreachable.

## Diagnostic firmware (`diag/`)

These configs don't use the rbAmp component. They probe the I2C pins directly.

| File | What it does |
| --- | --- |
| `monitor.yaml` | Every 100 ms: checks for pull-ups on D4/D5 (pins set to pull-down) and bit-bangs an address-0x50 ACK test. Logs `STATS` every 10 s. Healthy = `pullups_ok=100 ack_ok=100`. |
| `diag.yaml` | Pin survey plus bit-banged scans on D4/D5, with and without internal pull-ups, and swapped. |
| `float.yaml` | Leaves D4/D5 floating and logs their levels, to measure the rbAmp pull-up rail with a multimeter. |
| `idfscan.yaml` | Plain ESPHome/ESP-IDF I2C scan plus a 0x50 probe, with no rbAmp component. |
| `diag.h` | Shared helpers (`pin_survey`, `bb_scan`, `all_pairs`, `monitor_tick`, ...). |
