# Test log: XIAO ESP32-C3 + rbAmp UI1 + SCT013 5A/1V

Bench tests by Minjae, 12–13 Nov 2026 (NZDT). The firmware is in
[prototype/xiao-rbamp/](../prototype/xiao-rbamp/).

## Summary

| Function | Result | Evidence |
| --- | --- | --- |
| ESP32-C3 ↔ rbAmp I2C link | ✅ Working after a wiring fix on 13 Nov | 101/101 pull-up and ACK samples; module info read back |
| Current (clamp) | ✅ Reading, ⚠️ scale not verified | 0.44–0.50 A steady on a test load |
| Frequency | ✅ 50 Hz | Correct for NZ mains |
| Voltage | ❌ Not tested | 0.0 V: the L/N mains terminal was not connected |
| Power, power factor, energy | ❌ Not tested | Calculated from voltage, so all read ~0 |
| Accuracy against a reference meter | ❌ Not done | No reference reading taken |
| Web page over Wi-Fi | ✅ Working | Live event stream at 5 s intervals on a 2.4 GHz network |

**What this shows:** the module talks to the ESP, the clamp measures current, and the web page shows live readings.
**What it does not show yet:** voltage, power and energy (these need the L/N mains connection), and whether the current reading is accurate.

## Module info (boot log, 13 Nov)

| Field | Value |
| --- | --- |
| I2C address | 0x50 (found by bus scan) |
| Firmware | 0x04 (v1.3) |
| Register map | 8-bit |
| Topology | Single phase, 1 current channel, voltage: yes |
| Sensor class | SCT_013 (0x01) |
| Capabilities | 0x079E |
| Bidirectional | No |

## Readings (13 Nov, ~14:45–15:40 NZDT)

Test load: an appliance on an AC line splitter, with the clamp on the L conductor. L/N terminal not connected.

| Sensor | First run (15 s updates, ~50 s) | Second run, appliance on (35 s) |
| --- | --- | --- |
| Current | 0.444–0.451 A | 0.479–0.496 A |
| Voltage | 0.0 V | 0.0 V |
| Power | ~0 W | 0.0 W |
| Power factor | 0.000 | 0.000 |
| Frequency | 50 Hz | 50 Hz |
| Energy | 0.003 Wh | 0.007 Wh |

For scale: 0.49 A at 230 V would be about 110 W. This is **not verified**: `ct_model` could not be
written to this production module, so the stored CT model is in use. Check this against
[rbamp-esphome#1](https://github.com/rb-amp/rbamp-esphome/issues/1) (current reads 3× high when the
CT model setting doesn't take effect).

## Debug history: I2C link not working (12–13 Nov)

From 12 Nov to midday on 13 Nov, the ESP could not reliably talk to the rbAmp.

| Symptom | Seen |
| --- | --- |
| `Probe failed at 0x50 — no response`, bus scan `Found no devices` | Most boots |
| Module answered once (FW 0x04, caps 0x079E), but no live data (`RT block not ready`, `LATCH write failed`) | 12 Nov, once |
| `I2C bus is still busy` and phantom scan hits that changed every boot (0x17, 0x25, 0x27, 0x28, 0x61, 0x70, 0x72) | 13 Nov, after extra 4.7 kΩ pull-ups were added |
| `diag/monitor.yaml`: `pullups_ok=0 ack_ok=0` on every 10 s window ([log](evidence/2026-11-13-diag-monitor-no-pullups.log)) | 13 Nov, 10:02–14:30 |

### What was tested

| Test | Result |
| --- | --- |
| Continuity (multimeter) on Vin, GND, SDA, SCL | Pass |
| rbAmp supply | 5.12 V, steady |
| YAML against the official rbAmp example | Matches. `ct_model` disabled because the module refuses the write. |
| Spare rbAmp | Same fault |
| Spare XIAO | Same fault |
| Expansion board vs. breadboard | Same fault |
| All 110 GPIO pin pairs, bit-banged scan | Only D4=SDA / D5=SCL ever answered |
| Bus at 10 kHz instead of 50 kHz | No reliable improvement |
| External 4.7 kΩ pull-ups to 3V3 on D4/D5 | Not detected at the XIAO pins (0 of ~300 checks) |
| Wrong Wi-Fi password in `secrets.yaml` | Found and fixed |

### Resolution

On 13 Nov at ~14:37 the wiring was redone. Right after that, `diag/monitor.yaml` showed pull-ups present and
an ACK at 0x50 on **101 of 101** samples, and the meter firmware read the module info. The no-pull-up
pattern before that (no pull-up at all on D4/D5, even with the rbAmp's built-in 4.7 kΩ pair) fits an
rbAmp that wasn't powered or didn't share ground with the XIAO, or a lost connection between the XIAO
pins and the bus wires. Exactly which connection was wrong was not recorded.

Lessons:
- A single rbAmp does **not** need external pull-ups. The module has 4.7 kΩ on-board.
- A multimeter reading of 3.3 V on SDA/SCL proves nothing while ESPHome is running, because ESPHome turns on
  the ESP's internal pull-ups. Use `diag/monitor.yaml`, which pulls the pins *down*: only a real
  external pull-up then reads high.

## Wi-Fi and web page

- The C3 found no networks until the **FPC antenna** was fitted. Then it joined the 2.4 GHz network.
- The laptop couldn't reach the board at first. The cause was the laptop's Tailscale exit node routing LAN
  traffic through the tunnel, not router client isolation. Fixed with
  `tailscale set --exit-node-allow-lan-access=true`.
- Final setup: a dedicated 2.4 GHz IoT network. The web page (`http://rbamp-meter.local`) streams live
  readings every 5 s, with a pulsing "live" dot on the charts (`live-chart.js`).

## Still to do

1. Connect L/N to the rbAmp through a fused (≤0.1 A) lead, in an enclosure. This is mains work, so a qualified person must do it.
2. Verify voltage, power and energy against a plug-in reference meter on a resistive load (kettle or heater).
3. Verify the current scale (rbamp-esphome#1). If it's off by a constant factor, add a `multiply:` filter.
4. Move the bus back to 50 kHz and confirm it stays stable.
5. Add photos of the setup.
