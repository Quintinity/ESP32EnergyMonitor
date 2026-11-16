// Diagnostic helpers for XIAO ESP32-C3 + rbAmp (no I2C driver used).
#pragma once
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cstring>
#include "esphome/core/hal.h"
using esphome::App;

namespace diag {
static const char *const TAG = "diag";

// 1) Pull-down survey: a pin with an EXTERNAL pull-up (rbAmp 4.7k to its 3.3V) reads 1,
//    a floating/unconnected pin reads 0. Also read with no pull and with internal pull-up.
inline void pin_survey() {
  const int pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21};
  const char *names[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D8", "D9", "D10", "D7", "D6"};
  ESP_LOGI(TAG, "=== PIN SURVEY (pulldown=1 means an external pull-up is on that pin) ===");
  for (int i = 0; i < 11; i++) {
    gpio_num_t p = (gpio_num_t) pins[i];
    gpio_reset_pin(p);
    gpio_set_direction(p, GPIO_MODE_INPUT);
    gpio_set_pull_mode(p, GPIO_PULLDOWN_ONLY);
    esp_rom_delay_us(2000);
    int pd = gpio_get_level(p);
    gpio_set_pull_mode(p, GPIO_PULLUP_ONLY);
    esp_rom_delay_us(2000);
    int pu = gpio_get_level(p);
    gpio_set_pull_mode(p, GPIO_FLOATING);
    ESP_LOGI(TAG, "  %-3s GPIO%-2d  pulldown->%d  pullup->%d%s", names[i], pins[i], pd, pu,
             (pd == 1) ? "   <-- EXTERNAL PULL-UP" : (pu == 0 ? "   <-- HELD LOW" : ""));
  }
}

// 2) Bit-banged I2C scan, independent of the ESP-IDF I2C driver.
//    ~10 kHz, tolerant of clock stretching (waits up to 2 ms for SCL to rise).
struct BB {
  gpio_num_t sda, scl;
  bool stretch_timeout = false;
  void rel(gpio_num_t p) { gpio_set_level(p, 1); }
  void low(gpio_num_t p) { gpio_set_level(p, 0); }
  void d() { esp_rom_delay_us(50); }
  void scl_hi() {
    rel(scl);
    for (int i = 0; i < 2000 && !gpio_get_level(scl); i++) esp_rom_delay_us(1);
    if (!gpio_get_level(scl)) stretch_timeout = true;
  }
  void init(bool internal_pullup) {
    for (gpio_num_t p : {sda, scl}) {
      gpio_reset_pin(p);
      gpio_set_direction(p, GPIO_MODE_INPUT_OUTPUT_OD);
      gpio_set_pull_mode(p, internal_pullup ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
      rel(p);
    }
    d(); d();
  }
  void start() { rel(sda); scl_hi(); d(); low(sda); d(); low(scl); d(); }
  void stop() { low(sda); d(); scl_hi(); d(); rel(sda); d(); }
  bool write(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
      if (b & (1 << i)) rel(sda); else low(sda);
      d(); scl_hi(); d(); low(scl);
    }
    rel(sda); d(); scl_hi(); d();
    bool ack = gpio_get_level(sda) == 0;
    low(scl); d();
    return ack;
  }
};

inline void bb_scan(int sda, int scl, bool internal_pullup) {
  BB bb{(gpio_num_t) sda, (gpio_num_t) scl};
  bb.init(internal_pullup);
  int idle_sda = gpio_get_level((gpio_num_t) sda), idle_scl = gpio_get_level((gpio_num_t) scl);
  int found = 0;
  char list[200] = {0};
  for (int a = 0x08; a <= 0x77; a++) {
    bb.start();
    bool ack = bb.write((a << 1) | 0);
    bb.stop();
    if (ack) { found++; snprintf(list + strlen(list), sizeof(list) - strlen(list), " 0x%02X", a); }
  }
  ESP_LOGI(TAG, "BITBANG scan SDA=GPIO%d SCL=GPIO%d internal_pullup=%s idle(SDA=%d,SCL=%d) stretch_timeout=%s -> %s%s",
           sda, scl, internal_pullup ? "on" : "off", idle_sda, idle_scl, bb.stretch_timeout ? "YES" : "no",
           found ? "FOUND:" : "nothing", list);
}

// 3) Try EVERY pin pair as SDA/SCL (internal pull-ups on). Finds the rbAmp wherever it is wired.
inline void all_pairs() {
  const int pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21};
  int hits = 0;
  ESP_LOGI(TAG, "=== ALL-PAIRS SCAN (110 combinations) ===");
  for (int a : pins) for (int b : pins) {
    if (a == b) continue;
    BB bb{(gpio_num_t) a, (gpio_num_t) b};
    bb.init(true);
    if (!gpio_get_level((gpio_num_t) a) || !gpio_get_level((gpio_num_t) b)) continue;
    for (int ad = 0x08; ad <= 0x77; ad++) {
      bb.start(); bool ack = bb.write(ad << 1); bb.stop();
      if (ack && !bb.stretch_timeout) { ESP_LOGI(TAG, "  ACK at 0x%02X with SDA=GPIO%d SCL=GPIO%d", ad, a, b); hits++; }
    }
    gpio_reset_pin((gpio_num_t) a); gpio_reset_pin((gpio_num_t) b);
    App.feed_wdt();
  }
  ESP_LOGI(TAG, "ALL-PAIRS DONE: %d ack(s)", hits);
}

// 4) HOLD mode: D4/D5 inputs with pull-DOWN, so a multimeter shows only what the rbAmp side provides.
inline void hold_init() {
  for (gpio_num_t p : {GPIO_NUM_6, GPIO_NUM_7}) {
    gpio_reset_pin(p); gpio_set_direction(p, GPIO_MODE_INPUT); gpio_set_pull_mode(p, GPIO_PULLDOWN_ONLY);
  }
}
inline void hold_report() {
  ESP_LOGI(TAG, "HOLD: D4(GPIO6)=%d D5(GPIO7)=%d  (1 = rbAmp pull-up reaching the XIAO)",
           gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_7));
}

// 5) Which other pin, when its internal pull-up is ON, makes D4/D5 read high?
inline void phantom_test() {
  const int others[] = {2, 3, 4, 5, 8, 9, 10, 20, 21};
  auto all_down = [&]() {
    for (int o : others) { gpio_reset_pin((gpio_num_t) o); gpio_set_direction((gpio_num_t) o, GPIO_MODE_INPUT);
                           gpio_set_pull_mode((gpio_num_t) o, GPIO_PULLDOWN_ONLY); }
    for (gpio_num_t p : {GPIO_NUM_6, GPIO_NUM_7}) { gpio_reset_pin(p); gpio_set_direction(p, GPIO_MODE_INPUT);
                                                    gpio_set_pull_mode(p, GPIO_PULLDOWN_ONLY); }
  };
  all_down(); esphome::delay(300);
  ESP_LOGI(TAG, "PHANTOM baseline (all pulldown): D4=%d D5=%d", gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_7));
  for (int o : others) {
    all_down(); esphome::delay(100);
    gpio_set_pull_mode((gpio_num_t) o, GPIO_PULLUP_ONLY); esphome::delay(300);
    ESP_LOGI(TAG, "PHANTOM pull-up on GPIO%-2d -> D4=%d D5=%d  (that pin reads %d)", o,
             gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_7), gpio_get_level((gpio_num_t) o));
    App.feed_wdt();
  }
  all_down(); esphome::delay(300);
  ESP_LOGI(TAG, "PHANTOM end (all pulldown): D4=%d D5=%d", gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_7));
}

inline void run_all() {
  pin_survey();
  bb_scan(6, 7, false);  // D4=SDA, D5=SCL, rbAmp pull-ups only
  bb_scan(6, 7, true);   // same, plus internal pull-ups
  bb_scan(7, 6, true);   // swapped, in case SDA/SCL are reversed
}
}  // namespace diag

namespace diag {
// 6) Continuous monitor: every 100 ms check rbAmp presence (pull-ups on D4/D5) and ACK at 0x50.
struct Mon { uint32_t n = 0, pu_ok = 0, ack_ok = 0, t0 = 0; int last = -1; };
inline Mon &mon() { static Mon m; return m; }
inline void monitor_tick() {
  Mon &m = mon();
  hold_init(); esp_rom_delay_us(3000);
  int sda = gpio_get_level(GPIO_NUM_6), scl = gpio_get_level(GPIO_NUM_7);
  BB bb{GPIO_NUM_6, GPIO_NUM_7}; bb.init(true);  // internal pull-ups ON: no false ACKs
  bb.start(); bool ack = bb.write(0x50 << 1); bb.stop();
  int state = (sda && scl ? 2 : 0) + (ack ? 1 : 0);  // 3 = all good
  m.n++; if (sda && scl) m.pu_ok++; if (ack) m.ack_ok++;
  uint32_t now = esphome::millis();
  if (state != m.last) {
    ESP_LOGW(TAG, "CHANGE t=%.1fs  pullups(SDA=%d,SCL=%d)  ACK0x50=%s", now / 1000.0f, sda, scl, ack ? "yes" : "NO");
    m.last = state;
  }
  if (now - m.t0 >= 10000) {
    ESP_LOGI(TAG, "STATS last 10s: samples=%u pullups_ok=%u ack_ok=%u", m.n, m.pu_ok, m.ack_ok);
    m.n = m.pu_ok = m.ack_ok = 0; m.t0 = now;
  }
}
}  // namespace diag
