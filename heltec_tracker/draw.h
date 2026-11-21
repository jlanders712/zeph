#ifndef DRAW_H    // "Include guards" prevent the file from being loaded twice
#define DRAW_H

#include <Arduino.h> // Required so this file understands Arduino terms like 'pinMode'
#include <stdint.h>
#include <U8g2lib.h>
#include "config.h"
#include "state.h"
#include "battery.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;   // declared here, defined in draw.cpp

void drawInit();
void nextScreen(const ScreenData& d);
void updateCurrentScreen(const ScreenData& d);

void drawSleeping();

void drawBattery(int mv, int pct);

double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2);

#endif