#ifndef DRAW_H    // "Include guards" prevent the file from being loaded twice
#define DRAW_H

#include <Arduino.h> // Required so this file understands Arduino terms like 'pinMode'
#include <stdint.h>
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;   // declared here, defined in draw.cpp

void drawInit();

// Declare your function prototypes here
void drawArrow(int cx, int cy, int len, float bearingDeg);

// Draws the five status lines. The caller does clearBuffer() before and sendBuffer() after.
// sats_fix is the raw packet byte: bits 0-6 = satellites, bit 7 = valid fix.
void drawCoords(int seq, int sats_fix, int32_t lat_e7, int32_t lon_e7, float rssi, float snr);

void drawSleeping();

void drawBattery(int mv, int pct);

double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2);

#endif