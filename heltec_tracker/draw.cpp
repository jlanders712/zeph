#include "draw.h"
#include "config.h"
#include <math.h>
#include <U8g2lib.h>
#include <cmath>
#include <Wire.h>

bool screenPresent = false;
int lastButtonState = HIGH;
unsigned long pressStartTime = 0;
static int screenId = 0; // current screen index

// rotation, reset pin, SCL pin, SDA pin
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 21, 18, 17);

void drawInit() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, VEXT_ON_LEVEL);
  delay(100);

  pinMode(21, OUTPUT);                       // OLED reset: pulse it so the screen can answer
  digitalWrite(21, LOW);  delay(10);
  digitalWrite(21, HIGH); delay(10);

  Wire.begin(17, 18);                        // SDA, SCL
  Wire.beginTransmission(0x3C);
  screenPresent = (Wire.endTransmission() == 0);

  u8g2.begin();                              // always, so the font and buffer are valid
  u8g2.setFont(u8g2_font_6x10_tf);
}

// Draw an arrow centred at (cx, cy). bearingDeg: 0 = up, 90 = right, clockwise.
void drawArrow(int cx, int cy, int len, float bearingDeg) {
  if (!screenPresent) return;
  float a = bearingDeg * DEG_TO_RAD;
  float dx = sinf(a), dy = -cosf(a);          // unit vector (screen y points down)

  int tipX  = cx + dx * len / 2;
  int tipY  = cy + dy * len / 2;
  int tailX = cx - dx * len / 2;
  int tailY = cy - dy * len / 2;

  // arrowhead base sits 40% of the length back from the tip
  float hl = len * 0.4f;
  int bx = tipX - dx * hl;
  int by = tipY - dy * hl;
  // perpendicular direction for the head's width
  float hw = len * 0.22f;
  int lx = bx - dy * hw, ly = by + dx * hw;
  int rx = bx + dy * hw, ry = by - dx * hw;

  u8g2.drawLine(tailX, tailY, bx, by);        // shaft
  u8g2.drawTriangle(tipX, tipY, lx, ly, rx, ry);  // filled head
}

// Right-aligned in the bottom-right corner, on the same baseline as the last status line.
void drawBattery(int mv, int pct) {
  if (!screenPresent) return;
  char buf[16];
  if (pct < 0) snprintf(buf, sizeof(buf), "Bat --");
  else         snprintf(buf, sizeof(buf), "%d.%02dV %d%%", mv / 1000, (mv % 1000) / 10, pct);
  u8g2.drawStr(128 - u8g2.getStrWidth(buf), 58, buf);
}

void drawSleeping() {
  if (!screenPresent) return;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(34, 36, "Sleeping...");
  u8g2.sendBuffer();
}

// ============= Haversine distance
// Earth's radius in kilometers. Use 3956.0 for miles.
const double EARTH_RADIUS_KM = 6371.0;

// Helper function to convert degrees to radians
double toRadians(double degree) {
    return degree * M_PI / 180.0;
}

// Function to calculate Haversine distance
double calculateHaversineDistance(double lat1, double lon1, double lat2, double lon2) {
    // Convert latitude and longitude from degrees to radians
    double rLat1 = toRadians(lat1);
    double rLon1 = toRadians(lon1);
    double rLat2 = toRadians(lat2);
    double rLon2 = toRadians(lon2);

    // Differences in coordinates
    double dLat = rLat2 - rLat1;
    double dLon = rLon2 - rLon1;

    // Haversine formula
    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return EARTH_RADIUS_KM * c;
}

//============= screen orchestrators ===========================
// Note: printf's %f needs a double. The old version passed ints, which prints garbage.
void drawCoords(const ScreenData& d) {
  if (!screenPresent) return;
  if (d.trackerFound) & (d.satsFix & 0x80) {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);  u8g2.printf("Lat %.5f", d.lat_e7 / 1e7);
    u8g2.setCursor(0, 22);  u8g2.printf("Lon %.5f", d.lon_e7 / 1e7);
    u8g2.setCursor(0, 34);  u8g2.printf("Distance %.1f ft", calculateHaversineDistance(45.3, -114.6, 48.3, -113.6));
    u8g2.setCursor(0, 46);  u8g2.print("GPS fix");
    drawArrow(105, 32, 30, 107);
    u8g2.sendBuffer();
  }
  else if (!d.trackerFound) & (!(d.satsFix & 0x80)) {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);  u8g2.printf("Tracker Status");
    u8g2.setCursor(0, 34);  u8g2.print("No GPS fix");
    drawBattery(d.trackerMv, batteryPercent(d.trackerMv));
    u8g2.sendBuffer();
  }
  else {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Waiting for tracker");
    u8g2.sendBuffer();
  }
}

void drawTrackerInfo(const ScreenData& d) {
  if (!screenPresent) return;
  if (d.trackerFound) {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);  u8g2.printf("Tracker Status");
    u8g2.setCursor(0, 22);  u8g2.printf("Seq %d  Sats %d", d.seq, d.satsFix & 0x7F);
    u8g2.setCursor(0, 34);  u8g2.printf("RSSI %.0f SNR %.1f", d.rssi, d.snr);
    u8g2.setCursor(0, 46);  u8g2.print((d.satsFix & 0x80) ? "GPS fix" : "No GPS fix");
    drawBattery(d.trackerMv, batteryPercent(d.trackerMv));
    u8g2.sendBuffer();
  }
  else {
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);  u8g2.printf("Tracker Status");
    u8g2.drawStr(0, 34, "Waiting for tracker");
    u8g2.sendBuffer();
  }
}

void drawReceiverInfo(const ScreenData& d) {
  if (!screenPresent) return;
  u8g2.clearBuffer();
  u8g2.setCursor(0, 10);  u8g2.printf("Receiver Status");
  u8g2.setCursor(0, 22);  u8g2.printf("Received %d  Ignored %d", d.rxCount, d.rxBad);
  u8g2.setCursor(0, 34);  u8g2.printf("Radio %s ", d.radioOk ? "ok" : "NOT READY");
  drawBattery(d.rxMv, batteryPercent(d.rxMv));
  u8g2.sendBuffer();
}

typedef void (*ActionFunction)(const ScreenData&);
ActionFunction actions[] = { drawCoords, drawTrackerInfo, drawReceiverInfo };

const int numActions = sizeof(actions) / sizeof(actions[0]);

void nextScreen(const ScreenData& d) {
  screenId = (screenId + 1) % numActions;
  actions[screenId](d);
}

void updateCurrentScreen(const ScreenData& d) {
  actions[screenId](d);
}
