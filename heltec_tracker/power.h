#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include "config.h"

void powerInit();                              // call in setup(), after drawInit()
void powerPoll();                              // call every loop()
void powerSetBeforeSleep(void (*cb)());        // optional hook (radio.sleep(), GPS off, etc.)
void powerSetShortPress(void (*cb)());

#endif