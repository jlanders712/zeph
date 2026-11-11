#ifndef POWER_H
#define POWER_H

#include <Arduino.h>

#define BTN_PIN        0      // PRG button
#define LONG_PRESS_MS  1500   // hold this long to sleep

void powerInit();                              // call in setup(), after drawInit()
void powerPoll();                              // call every loop()
void powerSetBeforeSleep(void (*cb)());        // optional hook (radio.sleep(), GPS off, etc.)

#endif