// Heltec WiFi LoRa 32 V4 - custom GPS tracker (no Meshtastic)
// Libraries: RadioLib (8.x), TinyGPSPlus, NimBLE-Arduino (2.x), U8g2
// Board: Heltec "WiFi LoRa 32 V4", or ESP32S3 Dev Module with "USB CDC On Boot: Enabled".
// Hold the PRG button ~1.5 s to deep-sleep; press it again to wake (see power.cpp).

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "driver/gpio.h"
#include "config.h"
#include "draw.h"
#include "power.h"
#include "battery.h"
#include <TinyGPSPlus.h>

TinyGPSPlus gps;
HardwareSerial GPSSerial(1);

#if ROLE == ROLE_RECEIVER
  #include <NimBLEDevice.h>
  NimBLECharacteristic* bleChr = nullptr;
#endif

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

static bool radioOk = false;   // GPS logging works even when the radio failed to start
static uint32_t lastBlink = 0; // power on indicator

// ---------------- RF front-end control ----------------
static void femInit() {
  pinMode(FEM_POWER_PIN, OUTPUT);  digitalWrite(FEM_POWER_PIN, HIGH);
  pinMode(FEM_ENABLE_PIN, OUTPUT); digitalWrite(FEM_ENABLE_PIN, HIGH);
  pinMode(FEM_CTX_PIN, OUTPUT);    digitalWrite(FEM_CTX_PIN, LOW);
  delay(10);
}
static inline void femRx() { digitalWrite(FEM_CTX_PIN, LOW); }
static inline void femTx() { digitalWrite(FEM_CTX_PIN, HIGH); }

static bool radioInit() {
  femInit();
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  // RadioLib 8.x: begin() takes one config struct, frequency and bandwidth are integers in Hz,
  // and the TCXO voltage is a member that must be set BEFORE begin() (no longer an argument).
  radio.tcxoVoltage = RadioLibTCXOVoltage_t::Voltage1V8;   // Heltec boards use a 1.8 V TCXO

  ConfigLoRa_t cfg;
  cfg.frequency       = (uint32_t)lround(LORA_FREQ_MHZ * 1e6);   // MHz -> Hz
  cfg.bandwidth       = (uint32_t)lround(LORA_BW_KHZ * 1e3);     // kHz -> Hz
  cfg.spreadingFactor = LORA_SF;
  cfg.codingRate      = LORA_CR;           // denominator: 5 = 4/5
  cfg.syncWord        = LORA_SYNC;
  cfg.power           = LORA_POWER_DBM;
  cfg.preambleLength  = LORA_PREAMBLE;
  int st = radio.begin(cfg);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init failed, code %d\n", st);
    return false;
  }
  radio.setDio2AsRfSwitch(true);   // DIO2 drives the front-end TX/RX path pin
  Serial.println("LoRa ready");
  return true;
}

// ---------------- GPS ----------------
static void gpsInit() {
#if GPS_USE_VEXT
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, VEXT_ON_LEVEL);
#endif
#if GPS_STANDBY_PIN >= 0
  pinMode(GPS_STANDBY_PIN, OUTPUT);
  digitalWrite(GPS_STANDBY_PIN, HIGH);    // make sure the GPS isn't left in standby
#endif
  pinMode(GPS_EN_PIN, OUTPUT);
  digitalWrite(GPS_EN_PIN, LOW);          // active LOW = power on
  pinMode(GPS_RESET_PIN, OUTPUT);
  digitalWrite(GPS_RESET_PIN, LOW);       // reset pulse
  delay(150);
  digitalWrite(GPS_RESET_PIN, HIGH);
  delay(500);
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_UART_RX_PIN, GPS_UART_TX_PIN);
}

static void pumpGps() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    gps.encode(c);
#if GPS_DEBUG_RAW
    Serial.write(c);
#endif
  }
}

static void logGps(uint32_t now) {
  static uint32_t lastLog = 0;
  if (now - lastLog < 2000) return;
  lastLog = now;
  if (gps.charsProcessed() < 10) {
    Serial.printf("GPS: no NMEA data yet (%lu chars) - check GPS pins/power in config.h | radio %s\n",
                  (unsigned long)gps.charsProcessed(), radioOk ? "ok" : "NOT READY");
  } else {
    Serial.printf("GPS: fix=%d sats=%lu lat=%.6f lon=%.6f chars=%lu bad=%lu | radio %s\n",
                  gps.location.isValid(), (unsigned long)gps.satellites.value(),
                  gps.location.lat(), gps.location.lng(),
                  (unsigned long)gps.charsProcessed(), (unsigned long)gps.failedChecksum(),
                  radioOk ? "ok" : "NOT READY");
  }
}

static void logDistance(uint32_t now) {
  static uint32_t lastLog = 0;
  if (now - lastLog < 2000) return;
  lastLog = now;
  double dist = calculateHaversineDistance(45.3, -114.6, 48.3, -113.6);
  Serial.printf("Distance: %.2f",dist);
}

// ---------------- Status LED ----------------
#define LED_PIN 35   // white LED on the V4.2 (the V4-R8 uses GPIO46 instead); HIGH = on

static void ledBlink(uint32_t ms, uint32_t reps) {
  for (int i = 0; i < reps; i++) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

// ---------------- Deep sleep support ----------------
// Drive a pin to its "off" level and hold it there through deep sleep.
static void holdPin(int pin, int level) {
  if (pin < 0) return;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, level);
  gpio_hold_en((gpio_num_t)pin);
}
static void releasePin(int pin) {
  if (pin >= 0) gpio_hold_dis((gpio_num_t)pin);
}

// Called by power.cpp right before esp_deep_sleep_start().
static void prepareForSleep() {
  ledBlink(300,3);                       // confirms the long press registered: let go of PRG
#if ROLE == ROLE_RECEIVER
  NimBLEDevice::deinit(true);          // disconnect the phone cleanly
  drawSleeping();
  delay(800);
  u8g2.setPowerSave(1);                // OLED off
#endif
  if (radioOk) radio.sleep();
  GPSSerial.end();

  holdPin(LED_PIN, LOW);               // keep the LED off while asleep
  holdPin(FEM_CTX_PIN, LOW);
  holdPin(FEM_ENABLE_PIN, LOW);
  holdPin(FEM_POWER_PIN, LOW);
  holdPin(GPS_EN_PIN, HIGH);           // active LOW = on, so HIGH = off
  holdPin(GPS_RESET_PIN, LOW);         // don't drive an unpowered GPS from its pins
#if GPS_STANDBY_PIN >= 0
  holdPin(GPS_STANDBY_PIN, LOW);
#endif
#if GPS_USE_VEXT || ROLE == ROLE_RECEIVER
  holdPin(VEXT_PIN, !VEXT_ON_LEVEL);   // uses your config, no polarity guessing
#endif
}

// Call first thing after waking, before the init functions set pin levels again.
static void releaseHolds() {
  releasePin(LED_PIN);
  releasePin(FEM_CTX_PIN);
  releasePin(FEM_ENABLE_PIN);
  releasePin(FEM_POWER_PIN);
  releasePin(GPS_EN_PIN);
  releasePin(GPS_RESET_PIN);
#if GPS_STANDBY_PIN >= 0
  releasePin(GPS_STANDBY_PIN);
#endif
#if GPS_USE_VEXT || ROLE == ROLE_RECEIVER
  releasePin(VEXT_PIN);
#endif
}

// =====================================================================
#if ROLE == ROLE_TRACKER
// =====================================================================

static RTC_DATA_ATTR uint16_t seqNo = 0;   // survives deep sleep

static void sendPosition() {
  if (!radioOk) return;
  Pkt p = {};
  p.magic = PKT_MAGIC;
  p.node  = NODE_ID;
  p.seq   = seqNo++;
  p.batt_mv = (uint16_t)batteryMv();

  bool fix = gps.location.isValid() && gps.location.age() < 5000;
  if (fix) {
    p.lat_e7 = (int32_t)lround(gps.location.lat() * 1e7);
    p.lon_e7 = (int32_t)lround(gps.location.lng() * 1e7);
    if (gps.altitude.isValid()) p.alt_m = (int16_t)gps.altitude.meters();
    if (gps.speed.isValid())    p.speed_kmh = (uint8_t)constrain((int)gps.speed.kmph(), 0, 255);
  }
  uint32_t sats = gps.satellites.isValid() ? gps.satellites.value() : 0;
  p.sats_fix = (uint8_t)(min<uint32_t>(sats, 127) | (fix ? 0x80 : 0));

  femTx();
  int st = radio.transmit((uint8_t*)&p, sizeof(p));
  femRx();
  Serial.printf("TX seq=%u fix=%d sats=%lu -> %s\n", p.seq, fix, (unsigned long)sats,
                st == RADIOLIB_ERR_NONE ? "ok" : "FAILED");
}

void setup() {
  releaseHolds();
  ledBlink(100,5);                       // power-on / wake flash
  powerInit();
  powerSetBeforeSleep(prepareForSleep);

  Serial.begin(115200);
  // The V4 uses native USB: wait (up to 4 s) for the Serial Monitor so the first messages aren't lost.
  for (uint32_t t = millis(); !Serial && millis() - t < 4000; ) delay(10);
  Serial.println("\nTracker role");
  gpsInit();
  radioOk = radioInit();            // don't block here: GPS output must work even if the radio doesn't
}

void loop() {
  powerPoll();
  pumpGps();

  static uint32_t lastTx = 0, lastRadioTry = 0;
  uint32_t now = millis();

  if (!radioOk && now - lastRadioTry >= 5000) {   // keep retrying the radio in the background
    lastRadioTry = now;
    radioOk = radioInit();
  }

  logGps(now);

  if (now - lastTx >= TX_INTERVAL_MS) {
    lastTx = now;
    sendPosition();
  }
}

// =====================================================================
#else  // ROLE_RECEIVER
// =====================================================================

volatile bool rxFlag = false;
void IRAM_ATTR onRx() { rxFlag = true; }

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override { Serial.println("BLE connected"); }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    Serial.println("BLE disconnected");
    NimBLEDevice::startAdvertising();
  }
};

static void bleInit() {
  NimBLEDevice::init(BLE_NAME);
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new ServerCb());
  NimBLEService* svc = srv->createService(BLE_SERVICE_UUID);
  bleChr = svc->createCharacteristic(BLE_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->start();
  Serial.println("BLE advertising as " BLE_NAME);
}

static void startRx() {
  radio.setDio1Action(onRx);
  femRx();
  radio.startReceive();
}

void setup() {
  releaseHolds();
  ledBlink(100,5);                       // power-on / wake flash
  powerInit();
  powerSetBeforeSleep(prepareForSleep);

  Serial.begin(115200);
  for (uint32_t t = millis(); !Serial && millis() - t < 1500; ) delay(10);
  Serial.println("\nReceiver role");

  gpsInit();
  drawInit();                       // also switches Vext on for the OLED (VEXT_ON_LEVEL in config.h)
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Waiting for tracker");
  int bootMv = batteryMv();
  drawBattery(bootMv, batteryPercent(bootMv));
  // Layout tests, re-enable while tuning the screen:
  // drawArrow(105, 32, 30, 107);
  // drawCoords(1, 0x82, 463000000, -1119000000, -90, 5);
  u8g2.sendBuffer();

  radioOk = radioInit();            // no blocking loop: BLE, display and the sleep button keep working
  if (radioOk) startRx();
  bleInit();
}

void loop() {
  powerPoll();                      // must stay above the early returns below
  pumpGps();

  static uint32_t lastRadioTry = 0;
  uint32_t now = millis();

  if (now - lastBlink >= 10000) {
    lastBlink = now;
    ledBlink(100,1);
  }

  if (!radioOk && now - lastRadioTry >= 5000) {   // keep retrying the radio in the background
    lastRadioTry = now;
    radioOk = radioInit();
    if (radioOk) startRx();
  }

  logGps(now);
  logDistance(now);

  if (!radioOk || !rxFlag) return;
  rxFlag = false;

  uint8_t buf[32];
  size_t len = radio.getPacketLength();
  if (len > sizeof(buf)) len = sizeof(buf);
  int st = radio.readData(buf, len);
  float rssi = radio.getRSSI();
  float snr  = radio.getSNR();

  if (st == RADIOLIB_ERR_NONE && len == sizeof(Pkt) && buf[0] == PKT_MAGIC) {
    int mv = batteryMv();
    Pkt p;
    memcpy(&p, buf, sizeof(p));

    uint8_t out[sizeof(Pkt) + 2];
    memcpy(out, &p, sizeof(Pkt));
    out[sizeof(Pkt)]     = (uint8_t)(int8_t)constrain((int)rssi, -128, 0);
    out[sizeof(Pkt) + 1] = (uint8_t)(int8_t)constrain((int)lroundf(snr), -128, 127);
    bleChr->setValue(out, sizeof(out));
    bleChr->notify();

    Serial.printf("RX seq=%u lat=%.6f lon=%.6f rssi=%.0f snr=%.1f\n", p.seq,
                  p.lat_e7 / 1e7, p.lon_e7 / 1e7, rssi, snr);
    Serial.printf("Tracker batt=%uV %d%%\n", p.batt_mv / 1000, batteryPercent(p.batt_mv));
    u8g2.clearBuffer();
    drawCoords(p.seq, p.sats_fix, p.lat_e7, p.lon_e7, rssi, snr);
    drawBattery(mv, batteryPercent(mv));
    u8g2.sendBuffer();
  } else {
    Serial.printf("RX ignored (status %d, len %u)\n", st, (unsigned)len);
  }
  radio.startReceive();
}
#endif
