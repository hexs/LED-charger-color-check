#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include "web_page.h"

// ================= ตั้งค่า Wi-Fi =================
const char* WIFI_SSID = "AMD Takaya Office 2.4G";
const char* WIFI_PASS = "Takaya2026@";
// ================================================

#define I2C_SDA 4
#define I2C_SCL 5

// ใช้ 50MS เพื่อให้เหมาะกับการวัดหลอด LED และตอบสนอง Web API ได้เร็ว
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);
WebServer server(80);

// ตัวแปรเก็บค่าล่าสุด (Cache) เพื่อไม่ให้ API ค้างตอนอ่านเซนเซอร์
struct SensorData {
  uint16_t r = 0, g = 0, b = 0, c = 0;
  float red = 0, green = 0, blue = 0;
  uint16_t colorTemp = 0;
  uint16_t lux = 0;
  bool saturated = false;
  unsigned long timestamp = 0;
} data;

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 250; // อ่านเซนเซอร์ทุก 250ms

// ================= API Handlers =================
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleApiData() {
  String json = "{";
  json += "\"raw\":{\"r\":" + String(data.r) + ",\"g\":" + String(data.g) + ",\"b\":" + String(data.b) + ",\"c\":" + String(data.c) + "},";
  json += "\"rgb\":{\"r\":" + String((int)data.red) + ",\"g\":" + String((int)data.green) + ",\"b\":" + String((int)data.blue) + "},";
  json += "\"wavelengths_nm\":{\"b\":465,\"g\":525,\"r\":615},";
  json += "\"color_temp_k\":" + String(data.colorTemp) + ",";
  json += "\"lux\":" + String(data.lux) + ",";
  json += "\"saturated\":" + String(data.saturated ? "true" : "false") + ",";
  json += "\"timestamp_ms\":" + String(data.timestamp);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*"); // อนุญาตให้ดึงข้ามโดเมนได้
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!tcs.begin()) {
    Serial.println("ไม่พบเซนเซอร์ TCS34725 กรุณาตรวจสอบสาย I2C");
    while (1) delay(100);
  }
  Serial.println("พบเซนเซอร์ TCS34725 พร้อมใช้งาน");

  // เชื่อมต่อ Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("กำลังเชื่อมต่อ Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nเชื่อมต่อ Wi-Fi สำเร็จ!");
  Serial.print("เปิดหน้า Web App ได้ที่: http://");
  Serial.println(WiFi.localIP());
  Serial.print("เรียกดู API ได้ที่:      http://");
  Serial.print(WiFi.localIP());
  Serial.println("/api/data");

  // กำหนด Route ของ Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.begin();
}

void loop() {
  server.handleClient();

  // อัปเดตค่าเซนเซอร์ตามรอบเวลา
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();
    tcs.getRawData(&data.r, &data.g, &data.b, &data.c);
    tcs.getRGB(&data.red, &data.green, &data.blue);
    data.colorTemp = tcs.calculateColorTemperature_dn40(data.r, data.g, data.b, data.c);
    data.lux = tcs.calculateLux(data.r, data.g, data.b);
    // ค่าเต็มของ 50MS คือประมาณ 21504 (ถ้าใช้ 600MS คือ 65535)
    data.saturated = (data.c >= 20000 || data.r >= 20000 || data.g >= 20000 || data.b >= 20000);
    data.timestamp = millis();
  }
}