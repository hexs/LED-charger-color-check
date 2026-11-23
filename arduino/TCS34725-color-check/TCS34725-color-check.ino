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
const unsigned long WIFI_TIMEOUT_MS = 10000;  // รอต่อแต่ละตัวสูงสุด 10 วินาที
int currentWifiIdx = 0;
bool wifiConnecting = false;
bool wasWifiConnected = false;
unsigned long wifiAttemptStart = 0;
// =========================================================


Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);
WebServer server(80);

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
const unsigned long READ_INTERVAL = 250;

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
  data.r = 0;
  data.g = 0;
  data.b = 0;
  data.c = 0;
  data.red = 0;
  data.green = 0;
  data.blue = 0;
  data.colorTemp = 0;
  data.lux = 0;
  data.saturated = false;
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
    // รีเซ็ตกลับเป็น DHCP เมื่อสลับมาจาก Wi-Fi ที่ใช้ Static IP
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

// เรียกใน loop() เพื่อสลับ Wi-Fi อัตโนมัติหากสัญญาณหลุดโดยไม่บล็อกการทำงานของเซนเซอร์
void handleWifiFallback() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasWifiConnected) {
      wasWifiConnected = true;
      wifiConnecting = false;
      printWifiInfo();
    }
    return;
  }

  // กรณีเพิ่งหลุดจาก Wi-Fi ที่เคยต่ออยู่
  if (wasWifiConnected) {
    wasWifiConnected = false;
    Serial.println("Wi-Fi หลุดการเชื่อมต่อ! กำลังลองสลับไปเครือข่ายอื่น...");
    startWifiAttempt((currentWifiIdx + 1) % WIFI_COUNT);
    return;
  }

  // กรณียังไม่ติดและหมดเวลาของตัวปัจจุบันแล้ว ให้สลับไปตัวถัดไป
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
    Serial.println("พบเซนเซอร์ TCS34725 พร้อมใช้งาน");
  } else {
    data.connected = false;
    Serial.println("ยังไม่พบเซนเซอร์ TCS34725 (จะคอยตรวจสอบและเชื่อมต่อใหม่อัตโนมัติ)");
  }

  // ตั้งค่าโหมด Wi-Fi และไล่ลองต่อตามลำดับตอนเปิดเครื่อง
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
  server.on("/api/led", HTTP_GET, handleApiLed);
  server.begin();
}

void loop() {
  handleWifiFallback();
  server.handleClient();

  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();
    data.timestamp = millis();

    // 1. เช็กว่าเซนเซอร์ยังเสียบอยู่หรือไม่
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

    // 2. ถ้าเพิ่งเสียบกลับเข้ามาใหม่ ให้สั่ง begin() เพื่อปลุกเซนเซอร์
    if (!data.connected) {
      if (tcs.begin()) {
        data.connected = true;
        Serial.println("เชื่อมต่อเซนเซอร์ TCS34725 กลับคืนมาสำเร็จ!");
      } else {
        return;
      }
    }

    // 3. อ่านค่าตามปกติ
    tcs.getRawData(&data.r, &data.g, &data.b, &data.c);
    tcs.getRGB(&data.red, &data.green, &data.blue);
    data.colorTemp = tcs.calculateColorTemperature_dn40(data.r, data.g, data.b, data.c);
    data.lux = tcs.calculateLux(data.r, data.g, data.b);
    data.saturated = (data.c >= 20000 || data.r >= 20000 || data.g >= 20000 || data.b >= 20000);

    if (led.autoMode) {
      updateOnboardLED((uint8_t)data.red, (uint8_t)data.green, (uint8_t)data.blue);
    }
  }
}