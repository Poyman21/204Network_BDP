#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTTYPE DHT22
const int dhtPins[4] = {14, 27, 26, 25};   // DHT22 ต่อที่ GPIO
const int ledPins[4] = {32, 19, 33, 15};    // LED ต่อที่ GPIO

DHT dhtSensors[4] = {
  DHT(dhtPins[0], DHTTYPE),
  DHT(dhtPins[1], DHTTYPE),
  DHT(dhtPins[2], DHTTYPE),
  DHT(dhtPins[3], DHTTYPE),
};

float temperatures[4];

// WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.print("📶 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
  Serial.print("🔗 IP Address: "); Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("🔄 Connecting to MQTT...");
    if (client.connect("esp32-coop1")) {
      Serial.println("✅ MQTT connected");
    } else {
      Serial.print("❌ failed, rc="); Serial.print(client.state());
      Serial.println(" retry in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(22, 21); // ✅ เพิ่มสำหรับ OLED

  // เริ่มต้น DHT และ LED
  for (int i = 0; i < 4; i++) {
    dhtSensors[i].begin();
    pinMode(ledPins[i], OUTPUT);
    Serial.printf("✅ DHT Sensor %d เริ่มทำงานที่ GPIO %d\n", i + 1, dhtPins[i]);
  }

  setup_wifi();
  client.setServer("broker.hivemq.com", 1883);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("❌ ไม่พบหน้าจอ OLED");
    while (true);
  }
  Serial.println("✅ หน้าจอ OLED พร้อมใช้งาน");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ระบบเริ่มทำงาน...");
  display.display();
  delay(2000);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);

for (int i = 0; i < 4; i++) {
  float temp = dhtSensors[i].readTemperature();
  float hum = dhtSensors[i].readHumidity();
  String ledState = "OFF";

  if (!isnan(temp) && !isnan(hum)) {
    bool turnOn = temp < 30;
    digitalWrite(ledPins[i], turnOn ? HIGH : LOW);
    ledState = turnOn ? "ON" : "OFF";

    // แสดงบนจอ OLED
    display.setCursor(0, i * 16);  // แถวละ 16 px
    display.printf("C%d: %.1fC %.1f%% %s", i + 1, temp, hum, ledState.c_str());

    // ส่ง MQTT
    char topic[32];
    sprintf(topic, "chicken/coop%d/full", i + 1);

    char payload[128];
    snprintf(payload, sizeof(payload),
      "{\"coop\":\"%d\",\"temp\":%.1f,\"humidity\":%.1f,\"LED\":\"%s\"}",
      i + 1, temp, hum, ledState.c_str()
    );

    client.publish(topic, payload);
    Serial.printf("📤 Sent to %s: %s\n", topic, payload);
  } else {
    Serial.printf("❌ Coop %d: Sensor Error\n", i + 1);
    display.setCursor(0, i * 16);
    display.printf("C%d: Sensor Error", i + 1);
  }
}

display.display();  // ✅ แสดงผลจริง
Serial.println("------");
delay(10000);
}