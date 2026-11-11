#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

// Battery voltage in millivolts, or -1 if the reading looks like "no battery connected".
int batteryMv();

// Rough 0-100 estimate from a resting LiPo/Li-ion voltage, or -1 if mv is -1.
int batteryPercent(int mv);

#endif