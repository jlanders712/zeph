#pragma once
// ============================================================
//  Heltec WiFi LoRa 32 V4 - custom GPS tracker
//  Flash the SAME sketch to both boards, changing ROLE below.
// ============================================================

#define ROLE_TRACKER   1   // has GPS module, transmits position over LoRa
#define ROLE_RECEIVER  2   // receives LoRa, relays to phone over BLE
#define ROLE           ROLE_TRACKER      // <-- change to ROLE_RECEIVER for board #2

#define HAS_SCREEN 1    // 0 for boards without the OLED 

#define NODE_ID        1                 // identifies this tracker in packets

// ---------------- LoRa radio settings (must match on both boards) ----------------
#define LORA_FREQ_MHZ    915.0           // US 915 MHz board. Use the band your board was sold for.
#define LORA_BW_KHZ      125.0
#define LORA_SF          9               // 7 = faster/shorter range, 12 = slowest/longest range
#define LORA_CR          5               // coding rate 4/5
#define LORA_SYNC        0x12            // private network (Meshtastic uses 0x2B, so no crosstalk)
#define LORA_PREAMBLE    8
#define LORA_POWER_DBM   10              // SX1262 output BEFORE the front-end amplifier. Start low.
#define LORA_TCXO_V      1.8             // Heltec boards use a 1.8 V TCXO

#define TX_INTERVAL_MS   5000            // tracker send interval

// ---------------- SX1262 SPI pins (same as Heltec V3, V4 is pin-compatible) ----------------
#define LORA_NSS    8
#define LORA_SCK    9
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_RST    12
#define LORA_BUSY   13
#define LORA_DIO1   14

// ---------------- RF front-end (amplifier / LNA) ----------------
// Hardware revision: 3 = V4.3 (KCT8103L front-end), 2 = V4.2 (GC1109 front-end).
// Check the revision printed on your board / purchase page.
#define FEM_REV          2               // set for a V4.2 board; use 3 for a V4.3
#define FEM_POWER_PIN    7               // powers the front-end (HIGH = on)
#define FEM_ENABLE_PIN   2               // chip enable (HIGH = on)
#if FEM_REV == 3
  #define FEM_CTX_PIN    5               // LOW = receive with LNA, HIGH = transmit
#else
  #define FEM_CTX_PIN    46              // V4.2 PA control
#endif
// TX/RX path switching (CPS) is driven automatically by SX1262 DIO2.

// ---------------- GNSS connector (tracker role only) ----------------
// Pins taken from Meshtastic's heltec_v4 variant. Variants differ slightly, so
// if you get no NMEA text on Serial, check these first.
#define GPS_BAUD         9600
#define GPS_UART_RX_PIN  39              // ESP32 RX  <- GPS TX
#define GPS_UART_TX_PIN  38              // ESP32 TX  -> GPS RX
#define GPS_EN_PIN       34              // active LOW power enable
#define GPS_RESET_PIN    42              // pulled LOW briefly to reset L76K
#define GPS_STANDBY_PIN  40              // HIGH = keep GPS awake (Meshtastic's V4 variant uses this); -1 = don't touch
#define GPS_PPS_PIN      41              // unused here
#define GPS_DEBUG_RAW    0               // 1 = echo the raw text from the GPS to the Serial Monitor
#define GPS_USE_VEXT     0               // set 1 if the GPS stays dead; polarity varies by revision
#define VEXT_PIN         36
#define VEXT_ON_LEVEL    LOW

// ---------------- BLE (receiver role only) ----------------
#define BLE_NAME         "LoRaTrack-RX"
#define BLE_SERVICE_UUID "8a1f0001-5b7c-4a52-9c3e-0d4b6f2e7a10"
#define BLE_CHAR_UUID    "8a1f0002-5b7c-4a52-9c3e-0d4b6f2e7a10"

// ---------------- Over-the-air packet (16 bytes, little-endian) ----------------
#define PKT_MAGIC 0xA5
struct __attribute__((packed)) Pkt {
  uint8_t  magic;      // 0xA5
  uint8_t  node;       // tracker id
  uint16_t seq;        // increments each send
  int32_t  lat_e7;     // degrees * 1e7
  int32_t  lon_e7;
  int16_t  alt_m;      // metres
  uint8_t  speed_kmh;  // capped at 255
  uint8_t  sats_fix;   // bits 0-6 = satellites, bit 7 = valid fix
  uint16_t batt_mv;    // battery millivolts, 0 = unknown
};
// BLE notification to phone = Pkt (16 bytes) + int8 RSSI dBm + int8 SNR dB = 18 bytes
