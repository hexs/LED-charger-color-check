#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include "web_page.h"

#define I2C_SDA 4
#define I2C_SCL 5
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
    IPAddress(10, 87, 1, 89),
    IPAddress(10, 87, 1, 200),
    IPAddress(255, 255, 255, 0),
    IPAddress(8, 8, 8, 8),
    IPAddress(1, 1, 1, 1) },
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

// เริ่มต้นด้วย 614MS และ Gain 60X สำหรับหลอดไฟแสงน้อยมาก
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_60X);
WebServer server(80);

struct SensorConfig {
  int gain = 60;           // 1, 4, 16, 60
  int intMs = 614;         // 3 (2.4ms), 24, 50, 101, 154, 614
  uint16_t satLimit = 64000;
} cfg;

struct SensorData {
  bool connected = false;
  uint16_t r = 0, g = 0, b = 0, c = 0;
  float red = 0, green = 0, blue = 0;
  uint16_t colorTemp = 0;
  uint16_t lux = 0;
  bool saturated = false;
  unsigned long timestamp = 0;
} data;

struct LedState {
  bool autoMode = true;
  uint8_t r = 0, g = 0, b = 0;
} led;

unsigned long lastReadTime = 0;

void updateOnboardLED(uint8_t r, uint8_t g, uint8_t b) {
  led.r = r;
  led.g = g;
  led.b = b;
  rgbLedWrite(RGB_LED_PIN, r, g, b);
}

bool isSensorPresent() {
  Wire.beginTransmission(TCS34725_ADDRESS);
  return (Wire.endTransmission() == 0);
}

void clearSensorData() {
  data.r = 0; data.g = 0; data.b = 0; data.c = 0;
  data.red = 0; data.green = 0; data.blue = 0;
  data.colorTemp = 0;
  data.lux = 0;
  data.saturated = false;
}

// ปรับค่า Gain และ Integration Time แบบ Real-time
void applySensorConfig(int gainVal, int intMsVal) {
  if (gainVal >= 60)      cfg.gain = 60;
  else if (gainVal >= 16) cfg.gain = 16;
  else if (gainVal >= 4)  cfg.gain = 4;
  else                    cfg.gain = 1;

  if (intMsVal >= 600)      { cfg.intMs = 614; cfg.satLimit = 64000; }
  else if (intMsVal >= 150) { cfg.intMs = 154; cfg.satLimit = 64000; }
  else if (intMsVal >= 100) { cfg.intMs = 101; cfg.satLimit = 42000; }
  else if (intMsVal >= 50)  { cfg.intMs = 50;  cfg.satLimit = 20000; }
  else if (intMsVal >= 24)  { cfg.intMs = 24;  cfg.satLimit = 10000; }
  else                      { cfg.intMs = 3;   cfg.satLimit = 1000;  }

  if (!data.connected) return;

  switch (cfg.gain) {
    case 60: tcs.setGain(TCS34725_GAIN_60X); break;
    case 16: tcs.setGain(TCS34725_GAIN_16X); break;
    case 4:  tcs.setGain(TCS34725_GAIN_4X);  break;
    default: tcs.setGain(TCS34725_GAIN_1X);  break;
  }

  switch (cfg.intMs) {
    case 614: tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_614MS); break;
    case 154: tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_154MS); break;
    case 101: tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_101MS); break;
    case 50:  tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_50MS);  break;
    case 24:  tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_24MS);  break;
    default:  tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_2_4MS); break;
  }

  lastReadTime = millis(); // รอให้ครบ 1 รอบการรับแสงใหม่ก่อนอ่านค่าถัดไป
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
  Serial.print("SSID:    "); Serial.println(WiFi.SSID());
  Serial.print("Web App: http://"); Serial.println(WiFi.localIP());
  Serial.print("API:     http://"); Serial.print(WiFi.localIP()); Serial.println("/api/data");
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
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleApiData() {
  String json = "{";
  json += "\"sensor_connected\":" + String(data.connected ? "true" : "false") + ",";
  json += "\"wifi\":{\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"},";
  json += "\"config\":{\"gain\":" + String(cfg.gain) + ",\"integration_ms\":" + String(cfg.intMs) + ",\"sat_limit\":" + String(cfg.satLimit) + "},";
  json += "\"raw\":{\"r\":" + String(data.r) + ",\"g\":" + String(data.g) + ",\"b\":" + String(data.b) + ",\"c\":" + String(data.c) + "},";
  json += "\"rgb\":{\"r\":" + String((int)data.red) + ",\"g\":" + String((int)data.green) + ",\"b\":" + String((int)data.blue) + "},";
  json += "\"wavelengths_nm\":{\"b\":465,\"g\":525,\"r\":615},";
  json += "\"color_temp_k\":" + String(data.colorTemp) + ",";
  json += "\"lux\":" + String(data.lux) + ",";
  json += "\"saturated\":" + String(data.saturated ? "true" : "false") + ",";
  json += "\"led\":{\"mode\":\"" + String(led.autoMode ? "auto" : "manual") + "\",\"r\":" + String(led.r) + ",\"g\":" + String(led.g) + ",\"b\":" + String(led.b) + "},";
  json += "\"timestamp_ms\":" + String(data.timestamp);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// API ปรับ Gain และ Integration Time: /api/config?gain=60&int_ms=614
void handleApiConfig() {
  int newGain = server.hasArg("gain") ? server.arg("gain").toInt() : cfg.gain;
  int newInt  = server.hasArg("int_ms") ? server.arg("int_ms").toInt() : cfg.intMs;
  applySensorConfig(newGain, newInt);

  String json = "{\"status\":\"ok\",\"gain\":" + String(cfg.gain) + ",\"integration_ms\":" + String(cfg.intMs) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiLed() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "auto") {
      led.autoMode = true;
    } else if (mode == "manual") {
      led.autoMode = false;
    }
  }

  if (!led.autoMode && server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    uint8_t r = constrain(server.arg("r").toInt(), 0, 255);
    uint8_t g = constrain(server.arg("g").toInt(), 0, 255);
    uint8_t b = constrain(server.arg("b").toInt(), 0, 255);
    updateOnboardLED(r, g, b);
  }

  String json = "{\"status\":\"ok\",\"mode\":\"" + String(led.autoMode ? "auto" : "manual") + "\",\"r\":" + String(led.r) + ",\"g\":" + String(led.g) + ",\"b\":" + String(led.b) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  updateOnboardLED(0, 0, 0);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);

  if (tcs.begin()) {
    data.connected = true;
    applySensorConfig(cfg.gain, cfg.intMs);
    Serial.println("พบเซนเซอร์ TCS34725 พร้อมใช้งาน");
  } else {
    data.connected = false;
    Serial.println("ยังไม่พบเซนเซอร์ TCS34725 (จะคอยตรวจสอบและเชื่อมต่อใหม่อัตโนมัติ)");
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

  // ปรับรอบการอ่านตาม Integration Time เพื่อไม่ให้บล็อก Web Server
  unsigned long readInterval = (cfg.intMs + 25 > 250) ? (cfg.intMs + 25) : 250;

  if (millis() - lastReadTime >= readInterval) {
    lastReadTime = millis();
    data.timestamp = millis();

    if (!isSensorPresent()) {
      if (data.connected) {
        Serial.println("เซนเซอร์ TCS34725 หลุดการเชื่อมต่อ!");
      }
      data.connected = false;
      clearSensorData();
      if (led.autoMode) {
        updateOnboardLED(0, 0, 0);
      }
      return;
    }

    if (!data.connected) {
      if (tcs.begin()) {
        data.connected = true;
        applySensorConfig(cfg.gain, cfg.intMs);
        Serial.println("เชื่อมต่อเซนเซอร์ TCS34725 กลับคืนมาสำเร็จ!");
      }
      return;
    }

    // อ่านค่าจากรีจิสเตอร์โดยตรง (ไม่เรียก delay ซ้ำเหมือน getRawData/getRGB)
    data.c = tcs.read16(TCS34725_CDATAL);
    data.r = tcs.read16(TCS34725_RDATAL);
    data.g = tcs.read16(TCS34725_GDATAL);
    data.b = tcs.read16(TCS34725_BDATAL);

    if (data.c == 0) {
      data.red = 0;
      data.green = 0;
      data.blue = 0;
      data.colorTemp = 0;
      data.lux = 0;
    } else {
      data.red   = constrain((float)data.r / data.c * 255.0f, 0.0f, 255.0f);
      data.green = constrain((float)data.g / data.c * 255.0f, 0.0f, 255.0f);
      data.blue  = constrain((float)data.b / data.c * 255.0f, 0.0f, 255.0f);
      data.colorTemp = tcs.calculateColorTemperature_dn40(data.r, data.g, data.b, data.c);
      data.lux = tcs.calculateLux(data.r, data.g, data.b);
    }

    data.saturated = (data.c >= cfg.satLimit || data.r >= cfg.satLimit || data.g >= cfg.satLimit || data.b >= cfg.satLimit);

    if (led.autoMode) {
      updateOnboardLED((uint8_t)data.red, (uint8_t)data.green, (uint8_t)data.blue);
    }
  }
}