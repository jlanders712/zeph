#include "power.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static void (*beforeSleep)() = nullptr;

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
}

void powerPoll() {
  static uint32_t pressStart = 0;
  if (digitalRead(BTN_PIN) == LOW) {
    if (pressStart == 0) pressStart = millis();
    if (millis() - pressStart > LONG_PRESS_MS) goToSleep();
  } else {
    pressStart = 0;
  }
}