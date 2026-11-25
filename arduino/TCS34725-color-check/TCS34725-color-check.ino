#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include "web_page.h"

// ================= กำหนดขา I2C สำหรับเซนเซอร์ 2 ตัว =================
#define I2C1_SDA 4
#define I2C1_SCL 5
#define I2C2_SDA 6
#define I2C2_SCL 7
#define RGB_LED_PIN 48

// ================= โครงสร้างการตั้งค่า Wi-Fi =================
struct WifiProfile {
  const char* ssid;
  const char* pass;
  bool useStaticIP;
  IPAddress localIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
};

WifiProfile WIFI_LIST[] = {
  { "AMD Takaya Office 2.4G",
    "Takaya2026@",
    true,
    IPAddress(10, 87, 1, 90),
    IPAddress(10, 87, 1, 200),
    IPAddress(255, 255, 255, 0),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1) },
  { "AMD Takaya Office 2.4G",
    "Takaya2026@",
    true,
    IPAddress(10, 87, 1, 89),
    IPAddress(10, 87, 1, 200),
    IPAddress(255, 255, 255, 0),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1) },
  { "Handwork_2.4G",
    "Takaya2026@",
    true,
    IPAddress(10, 87, 1, 90),
    IPAddress(10, 87, 1, 200),
    IPAddress(255, 255, 255, 0),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1) }
  { "Handwork_2.4G",
    "Takaya2026@",
    true,
    IPAddress(10, 87, 1, 89),
    IPAddress(10, 87, 1, 200),
    IPAddress(255, 255, 255, 0),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1) }
};

const int WIFI_COUNT = sizeof(WIFI_LIST) / sizeof(WIFI_LIST[0]);
const unsigned long WIFI_TIMEOUT_MS = 10000;
int currentWifiIdx = 0;
bool wifiConnecting = false;
bool wasWifiConnected = false;
unsigned long wifiAttemptStart = 0;
// =========================================================

Adafruit_TCS34725 tcs1 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_60X);
Adafruit_TCS34725 tcs2 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_60X);

Adafruit_TCS34725* tcsList[2] = { &tcs1, &tcs2 };
TwoWire* wireList[2] = { &Wire, &Wire1 };
const int SENSOR_PINS[2][2] = { { I2C1_SDA, I2C1_SCL }, { I2C2_SDA, I2C2_SCL } };

WebServer server(80);

struct SensorConfig {
  int gain = 60;    // 1, 4, 16, 60
  int intMs = 614;  // 3 (2.4ms), 24, 50, 101, 154, 614
  uint16_t satLimit = 64000;
} cfg;

struct SensorData {
  bool connected = false;
  uint16_t r = 0, g = 0, b = 0, c = 0;
  float red = 0, green = 0, blue = 0;
  uint16_t colorTemp = 0;
  uint16_t lux = 0;
  bool saturated = false;
};

SensorData sensors[2];
unsigned long lastTimestamp = 0;
unsigned long lastReadTime = 0;

struct LedState {
  String mode = "auto";  // "auto" (ตาม S1), "auto2" (ตาม S2), "manual"
  uint8_t r = 0, g = 0, b = 0;
} led;

void updateOnboardLED(uint8_t r, uint8_t g, uint8_t b) {
  led.r = r;
  led.g = g;
  led.b = b;
  rgbLedWrite(RGB_LED_PIN, r, g, b);
}

bool isSensorPresent(int idx) {
  wireList[idx]->beginTransmission(TCS34725_ADDRESS);
  return (wireList[idx]->endTransmission() == 0);
}

void clearSensorData(int idx) {
  sensors[idx].r = 0;
  sensors[idx].g = 0;
  sensors[idx].b = 0;
  sensors[idx].c = 0;
  sensors[idx].red = 0;
  sensors[idx].green = 0;
  sensors[idx].blue = 0;
  sensors[idx].colorTemp = 0;
  sensors[idx].lux = 0;
  sensors[idx].saturated = false;
}

void applySensorConfigTo(int idx) {
  if (!sensors[idx].connected) return;
  Adafruit_TCS34725* tcs = tcsList[idx];

  switch (cfg.gain) {
    case 60: tcs->setGain(TCS34725_GAIN_60X); break;
    case 16: tcs->setGain(TCS34725_GAIN_16X); break;
    case 4: tcs->setGain(TCS34725_GAIN_4X); break;
    default: tcs->setGain(TCS34725_GAIN_1X); break;
  }

  switch (cfg.intMs) {
    case 614: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_614MS); break;
    case 154: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_154MS); break;
    case 101: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_101MS); break;
    case 50: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_50MS); break;
    case 24: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_24MS); break;
    default: tcs->setIntegrationTime(TCS34725_INTEGRATIONTIME_2_4MS); break;
  }
}

void applySensorConfig(int gainVal, int intMsVal) {
  if (gainVal >= 60) cfg.gain = 60;
  else if (gainVal >= 16) cfg.gain = 16;
  else if (gainVal >= 4) cfg.gain = 4;
  else cfg.gain = 1;

  if (intMsVal >= 600) {
    cfg.intMs = 614;
    cfg.satLimit = 64000;
  } else if (intMsVal >= 150) {
    cfg.intMs = 154;
    cfg.satLimit = 64000;
  } else if (intMsVal >= 100) {
    cfg.intMs = 101;
    cfg.satLimit = 42000;
  } else if (intMsVal >= 50) {
    cfg.intMs = 50;
    cfg.satLimit = 20000;
  } else if (intMsVal >= 24) {
    cfg.intMs = 24;
    cfg.satLimit = 10000;
  } else {
    cfg.intMs = 3;
    cfg.satLimit = 1000;
  }

  for (int i = 0; i < 2; i++) {
    applySensorConfigTo(i);
  }
  lastReadTime = millis();
}

// ================= ฟังก์ชันจัดการสลับ Wi-Fi =================
void startWifiAttempt(int idx) {
  currentWifiIdx = idx;
  WifiProfile& p = WIFI_LIST[idx];

  WiFi.disconnect(false);
  delay(100);

  if (p.useStaticIP) {
    if (!WiFi.config(p.localIP, p.gateway, p.subnet, p.dns1, p.dns2)) {
      Serial.println("ตั้งค่า Static IP ไม่สำเร็จ!");
    }
  } else {
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  }

  Serial.print("\nกำลังลองเชื่อมต่อ Wi-Fi: ");
  Serial.println(p.ssid);

  WiFi.begin(p.ssid, p.pass);
  wifiConnecting = true;
  wifiAttemptStart = millis();
}

void printWifiInfo() {
  Serial.println("\nเชื่อมต่อ Wi-Fi สำเร็จ!");
  Serial.print("SSID:    ");
  Serial.println(WiFi.SSID());
  Serial.print("Web App: http://");
  Serial.println(WiFi.localIP());
  Serial.print("API:     http://");
  Serial.print(WiFi.localIP());
  Serial.println("/api/data");
}

void handleWifiFallback() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasWifiConnected) {
      wasWifiConnected = true;
      wifiConnecting = false;
      printWifiInfo();
    }
    return;
  }

  if (wasWifiConnected) {
    wasWifiConnected = false;
    Serial.println("Wi-Fi หลุดการเชื่อมต่อ! กำลังลองสลับไปเครือข่ายอื่น...");
    startWifiAttempt((currentWifiIdx + 1) % WIFI_COUNT);
    return;
  }

  if (!wifiConnecting || (millis() - wifiAttemptStart >= WIFI_TIMEOUT_MS)) {
    int nextIdx = wifiConnecting ? ((currentWifiIdx + 1) % WIFI_COUNT) : 0;
    startWifiAttempt(nextIdx);
  }
}

// ================= API Handlers =================
String sensorToJson(int idx) {
  SensorData& s = sensors[idx];
  String j = "{";
  j += "\"id\":" + String(idx + 1) + ",";
  j += "\"sda\":" + String(SENSOR_PINS[idx][0]) + ",";
  j += "\"scl\":" + String(SENSOR_PINS[idx][1]) + ",";
  j += "\"connected\":" + String(s.connected ? "true" : "false") + ",";
  j += "\"raw\":{\"r\":" + String(s.r) + ",\"g\":" + String(s.g) + ",\"b\":" + String(s.b) + ",\"c\":" + String(s.c) + "},";
  j += "\"rgb\":{\"r\":" + String((int)s.red) + ",\"g\":" + String((int)s.green) + ",\"b\":" + String((int)s.blue) + "},";
  j += "\"color_temp_k\":" + String(s.colorTemp) + ",";
  j += "\"lux\":" + String(s.lux) + ",";
  j += "\"saturated\":" + String(s.saturated ? "true" : "false");
  j += "}";
  return j;
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleApiData() {
  SensorData& s1 = sensors[0];
  String json = "{";
  json += "\"sensor_connected\":" + String(s1.connected ? "true" : "false") + ",";
  json += "\"wifi\":{\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"},";
  json += "\"config\":{\"gain\":" + String(cfg.gain) + ",\"integration_ms\":" + String(cfg.intMs) + ",\"sat_limit\":" + String(cfg.satLimit) + "},";
  json += "\"raw\":{\"r\":" + String(s1.r) + ",\"g\":" + String(s1.g) + ",\"b\":" + String(s1.b) + ",\"c\":" + String(s1.c) + "},";
  json += "\"rgb\":{\"r\":" + String((int)s1.red) + ",\"g\":" + String((int)s1.green) + ",\"b\":" + String((int)s1.blue) + "},";
  json += "\"wavelengths_nm\":{\"b\":465,\"g\":525,\"r\":615},";
  json += "\"color_temp_k\":" + String(s1.colorTemp) + ",";
  json += "\"lux\":" + String(s1.lux) + ",";
  json += "\"saturated\":" + String(s1.saturated ? "true" : "false") + ",";
  json += "\"sensors\":[" + sensorToJson(0) + "," + sensorToJson(1) + "],";
  json += "\"led\":{\"mode\":\"" + led.mode + "\",\"r\":" + String(led.r) + ",\"g\":" + String(led.g) + ",\"b\":" + String(led.b) + "},";
  json += "\"timestamp_ms\":" + String(lastTimestamp);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiConfig() {
  int newGain = server.hasArg("gain") ? server.arg("gain").toInt() : cfg.gain;
  int newInt = server.hasArg("int_ms") ? server.arg("int_ms").toInt() : cfg.intMs;
  applySensorConfig(newGain, newInt);

  String json = "{\"status\":\"ok\",\"gain\":" + String(cfg.gain) + ",\"integration_ms\":" + String(cfg.intMs) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiLed() {
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    if (m == "auto" || m == "auto1") {
      led.mode = "auto";
    } else if (m == "auto2") {
      led.mode = "auto2";
    } else if (m == "manual") {
      led.mode = "manual";
    }
  }

  if (led.mode == "manual" && server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    uint8_t r = constrain(server.arg("r").toInt(), 0, 255);
    uint8_t g = constrain(server.arg("g").toInt(), 0, 255);
    uint8_t b = constrain(server.arg("b").toInt(), 0, 255);
    updateOnboardLED(r, g, b);
  }

  String json = "{\"status\":\"ok\",\"mode\":\"" + led.mode + "\",\"r\":" + String(led.r) + ",\"g\":" + String(led.g) + ",\"b\":" + String(led.b) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  updateOnboardLED(0, 0, 0);

  Wire.begin(I2C1_SDA, I2C1_SCL);
  Wire.setTimeOut(50);

  Wire1.begin(I2C2_SDA, I2C2_SCL);
  Wire1.setTimeOut(50);

  for (int i = 0; i < 2; i++) {
    if (tcsList[i]->begin(TCS34725_ADDRESS, wireList[i])) {
      sensors[i].connected = true;
      applySensorConfigTo(i);
      Serial.printf("พบเซนเซอร์ #%d (SDA:%d, SCL:%d) พร้อมใช้งาน\n", i + 1, SENSOR_PINS[i][0], SENSOR_PINS[i][1]);
    } else {
      sensors[i].connected = false;
      Serial.printf("ยังไม่พบเซนเซอร์ #%d (SDA:%d, SCL:%d) — จะรอเชื่อมต่ออัตโนมัติ\n", i + 1, SENSOR_PINS[i][0], SENSOR_PINS[i][1]);
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);

  for (int i = 0; i < WIFI_COUNT; i++) {
    startWifiAttempt(i);
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiAttemptStart < WIFI_TIMEOUT_MS)) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wasWifiConnected = true;
      wifiConnecting = false;
      printWifiInfo();
      break;
    }
    Serial.println("\nเชื่อมต่อไม่สำเร็จ กำลังลองตัวถัดไป...");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ยังไม่สามารถเชื่อมต่อ Wi-Fi ได้ทั้งคู่ (จะลองเชื่อมต่อใหม่อัตโนมัติในเบื้องหลัง)");
    wifiConnecting = false;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/config", HTTP_GET, handleApiConfig);
  server.on("/api/led", HTTP_GET, handleApiLed);
  server.begin();
}

void loop() {
  handleWifiFallback();
  server.handleClient();

  unsigned long readInterval = (cfg.intMs + 25 > 250) ? (cfg.intMs + 25) : 250;

  if (millis() - lastReadTime >= readInterval) {
    lastReadTime = millis();
    lastTimestamp = millis();

    for (int i = 0; i < 2; i++) {
      if (!isSensorPresent(i)) {
        if (sensors[i].connected) {
          Serial.printf("เซนเซอร์ #%d หลุดการเชื่อมต่อ!\n", i + 1);
        }
        sensors[i].connected = false;
        clearSensorData(i);
        continue;
      }

      if (!sensors[i].connected) {
        if (tcsList[i]->begin(TCS34725_ADDRESS, wireList[i])) {
          sensors[i].connected = true;
          applySensorConfigTo(i);
          Serial.printf("เชื่อมต่อเซนเซอร์ #%d กลับคืนมาสำเร็จ!\n", i + 1);
        }
        continue;
      }

      Adafruit_TCS34725* tcs = tcsList[i];
      SensorData& s = sensors[i];

      s.c = tcs->read16(TCS34725_CDATAL);
      s.r = tcs->read16(TCS34725_RDATAL);
      s.g = tcs->read16(TCS34725_GDATAL);
      s.b = tcs->read16(TCS34725_BDATAL);

      if (s.c == 0) {
        s.red = 0;
        s.green = 0;
        s.blue = 0;
        s.colorTemp = 0;
        s.lux = 0;
      } else {
        s.red = constrain((float)s.r / s.c * 255.0f, 0.0f, 255.0f);
        s.green = constrain((float)s.g / s.c * 255.0f, 0.0f, 255.0f);
        s.blue = constrain((float)s.b / s.c * 255.0f, 0.0f, 255.0f);
        s.colorTemp = tcs->calculateColorTemperature_dn40(s.r, s.g, s.b, s.c);
        s.lux = tcs->calculateLux(s.r, s.g, s.b);
      }

      s.saturated = (s.c >= cfg.satLimit || s.r >= cfg.satLimit || s.g >= cfg.satLimit || s.b >= cfg.satLimit);
    }

    if (led.mode == "auto") {
      if (sensors[0].connected) updateOnboardLED((uint8_t)sensors[0].red, (uint8_t)sensors[0].green, (uint8_t)sensors[0].blue);
      else updateOnboardLED(0, 0, 0);
    } else if (led.mode == "auto2") {
      if (sensors[1].connected) updateOnboardLED((uint8_t)sensors[1].red, (uint8_t)sensors[1].green, (uint8_t)sensors[1].blue);
      else updateOnboardLED(0, 0, 0);
    }
  }
}