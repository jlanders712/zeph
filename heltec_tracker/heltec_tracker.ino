// Heltec WiFi LoRa 32 V4 - custom GPS tracker (no Meshtastic)
// Libraries: RadioLib (7.x), TinyGPSPlus, NimBLE-Arduino (2.x)
// Board: Heltec "WiFi LoRa 32 V4", or ESP32S3 Dev Module with "USB CDC On Boot: Enabled".

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "config.h"

#if ROLE == ROLE_TRACKER
  #include <TinyGPSPlus.h>
  TinyGPSPlus gps;
  HardwareSerial GPSSerial(1);
#else
  #include <NimBLEDevice.h>
  NimBLECharacteristic* bleChr = nullptr;
#endif

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

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
  int st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC,
                       LORA_POWER_DBM, LORA_PREAMBLE, LORA_TCXO_V);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init failed, code %d\n", st);
    return false;
  }
  radio.setDio2AsRfSwitch(true);   // DIO2 drives the front-end TX/RX path pin
  Serial.println("LoRa ready");
  return true;
}

// =====================================================================
#if ROLE == ROLE_TRACKER
// =====================================================================

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

static uint16_t seqNo = 0;
static bool radioOk = false;   // GPS logging works even when the radio failed to start

static void sendPosition() {
  if (!radioOk) return;
  Pkt p = {};
  p.magic = PKT_MAGIC;
  p.node  = NODE_ID;
  p.seq   = seqNo++;

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
  Serial.begin(115200);
  // The V4 uses native USB: wait (up to 4 s) for the Serial Monitor so the first messages aren't lost.
  for (uint32_t t = millis(); !Serial && millis() - t < 4000; ) delay(10);
  Serial.println("\nTracker role");
  gpsInit();
  radioOk = radioInit();            // don't block here: GPS output must work even if the radio doesn't
}

void loop() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    gps.encode(c);
#if GPS_DEBUG_RAW
    Serial.write(c);
#endif
  }

  static uint32_t lastTx = 0, lastLog = 0, lastRadioTry = 0;
  uint32_t now = millis();

  if (!radioOk && now - lastRadioTry >= 5000) {   // keep retrying the radio in the background
    lastRadioTry = now;
    radioOk = radioInit();
  }

  if (now - lastLog >= 2000) {
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

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nReceiver role");
  while (!radioInit()) delay(2000);
  radio.setDio1Action(onRx);
  femRx();
  radio.startReceive();
  bleInit();
}

void loop() {
  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[32];
  size_t len = radio.getPacketLength();
  if (len > sizeof(buf)) len = sizeof(buf);
  int st = radio.readData(buf, len);

  if (st == RADIOLIB_ERR_NONE && len == sizeof(Pkt) && buf[0] == PKT_MAGIC) {
    uint8_t out[sizeof(Pkt) + 2];
    memcpy(out, buf, sizeof(Pkt));
    out[sizeof(Pkt)]     = (uint8_t)(int8_t)constrain((int)radio.getRSSI(), -128, 0);
    out[sizeof(Pkt) + 1] = (uint8_t)(int8_t)constrain((int)lroundf(radio.getSNR()), -128, 127);
    bleChr->setValue(out, sizeof(out));
    bleChr->notify();
    Pkt* p = (Pkt*)buf;
    Serial.printf("RX seq=%u lat=%.6f lon=%.6f rssi=%.0f snr=%.1f\n", p->seq,
                  p->lat_e7 / 1e7, p->lon_e7 / 1e7, radio.getRSSI(), radio.getSNR());
  } else {
    Serial.printf("RX ignored (status %d, len %u)\n", st, (unsigned)len);
  }
  radio.startReceive();
}
#endif
