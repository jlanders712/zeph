#include "power.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static void (*beforeSleep)() = nullptr;
static void (*onShortPress)() = nullptr;
void powerSetShortPress(void (*cb)()) { onShortPress = cb; }
void powerSetBeforeSleep(void (*cb)()) { beforeSleep = cb; }

static void goToSleep() {
  if (beforeSleep) beforeSleep();

  gpio_deep_sleep_hold_en();                       // keep held pins held while asleep

  while (digitalRead(BTN_PIN) == LOW) delay(10);   // wait for release or we wake instantly
  delay(100);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);     // wake when PRG goes LOW
  esp_deep_sleep_start();
}

void powerInit() {
  gpio_deep_sleep_hold_dis();
  pinMode(BTN_PIN, INPUT_PULLUP);

  // Finger is probably still on the button after waking
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    while (digitalRead(BTN_PIN) == LOW) delay(10);
    delay(100);
  }
  for (uint32_t t = millis(); digitalRead(BTN_PIN) == LOW && millis() - t < 3000; ) delay(10); //avoid changing screens on wakeup
}

void powerPoll() {
  static bool down = false;
  static uint32_t pressStart = 0, lastChange = 0;
  uint32_t now = millis();
  bool low = (digitalRead(BTN_PIN) == LOW);

  if (low && !down && now - lastChange > 30) {            // pressed (30 ms debounce)
    down = true; pressStart = now; lastChange = now;
  } else if (!low && down && now - lastChange > 30) {     // released
    down = false; lastChange = now;
    if (now - pressStart < LONG_PRESS_MS && onShortPress) onShortPress();
  } else if (low && down && now - pressStart > LONG_PRESS_MS) {
    goToSleep();                                          // long press, still held
  }
}