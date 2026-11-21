// state.h
#pragma once
#include <Arduino.h>

struct ScreenData {
  bool     trackerFound = false;
  // last tracker packet
  int      seq = 0;
  int      satsFix = 0;
  int32_t  lat_e7 = 0, lon_e7 = 0;
  float    rssi = 0, snr = 0;
  uint16_t trackerMv = 0;
  // receiver's own
  int      rxMv = 0;
  uint16_t rxCount = 0, rxBad = 0;
  bool     radioOk = false;
};