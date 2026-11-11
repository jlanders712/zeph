#include "battery.h"

// Heltec WiFi LoRa 32 V4 (S3R2 boards, V4.2 / V4.3): the battery is sensed on GPIO1 (ADC1_CH0)
// through a divider that is only enabled while ADC_Ctrl (GPIO37) is driven HIGH.
// Heltec's documented relation: VBAT = VADC * 490 / 100.
#define BAT_ADC_PIN    1
#define BAT_CTRL_PIN   37
#define BAT_DIVIDER    4.9f     // adjust against a multimeter if the reading is a bit off

int batteryMv() {
  pinMode(BAT_CTRL_PIN, OUTPUT);
  digitalWrite(BAT_CTRL_PIN, HIGH);        // enable the divider
  delay(5);                                // let it settle

  const int N = 16;
  uint32_t sum = 0;
  for (int i = 0; i < N; i++) {
    sum += analogReadMilliVolts(BAT_ADC_PIN);   // factory-calibrated millivolts at the pin
    delayMicroseconds(200);
  }
  digitalWrite(BAT_CTRL_PIN, LOW);         // disable the divider again so it doesn't draw current

  int mv = (int)lroundf((sum / (float)N) * BAT_DIVIDER);
  return (mv < 2500) ? -1 : mv;            // nothing sensible below this: treat as no battery
}

// Approximate resting voltage -> percent curve for a single LiPo cell (highest voltage first).
// Under load the voltage sags, and while charging it reads high, so treat this as an estimate.
int batteryPercent(int mv) {
  if (mv < 0) return -1;
  static const struct { int mv; int pct; } curve[] = {
    {4200, 100}, {4060, 90}, {3980, 80}, {3920, 70}, {3870, 60}, {3820, 50},
    {3790, 40},  {3750, 30}, {3700, 20}, {3600, 10}, {3300, 0},
  };
  const int n = sizeof(curve) / sizeof(curve[0]);
  if (mv >= curve[0].mv) return 100;
  for (int i = 1; i < n; i++) {
    if (mv >= curve[i].mv) {
      int span = curve[i - 1].mv - curve[i].mv;
      return curve[i].pct + (curve[i - 1].pct - curve[i].pct) * (mv - curve[i].mv) / span;
    }
  }
  return 0;
}