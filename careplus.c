
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* MQTT_BROKER = "970831cd60a54d73ac2e8cc5818c9888.s1.eu.hivemq.cloud";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "challenge_iot";
const char* MQTT_PASS   = "Challenge_iot123";
const char* DEVICE_ID   = "careplus-device-001";

#define TOPIC_METRICS "careplus/careplus-device-001/metrics"
#define TOPIC_STATUS  "careplus/careplus-device-001/status"
#define TOPIC_ALERT   "careplus/careplus-device-001/alert"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_I2C_ADDR  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClientSecure espClient;
PubSubClient     mqtt(espClient);

struct HealthMetrics {
  float heartRate;
  float spo2;
  float temperature;
  int   steps;
  int   battery;
  bool  alertActive;
};

HealthMetrics metrics = {72.0, 98.0, 36.6, 0, 100, false};

unsigned long lastPublishMs = 0;
unsigned long lastSensorMs  = 0;
unsigned long lastDisplayMs = 0;
const long PUBLISH_INTERVAL = 5000;
const long SENSOR_INTERVAL  = 1000;
const long DISPLAY_INTERVAL = 2000;

int   currentScreen = 0;
float hrDrift       = 0.0;
float tempDrift     = 0.0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== CarePlus Health Tracker v2 ===");

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("[ERRO] OLED nao encontrado");
    while (true);
  }

  showSplash();
  connectWiFi();

  espClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(60);
  mqtt.setBufferSize(512);   

  Serial.println("[OK] Setup concluido");
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  unsigned long now = millis();

  if (now - lastSensorMs >= SENSOR_INTERVAL) {
    lastSensorMs = now;
    updateSimulatedSensors();
    checkAlerts();
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL) {
    lastDisplayMs = now;
    updateDisplay();
    currentScreen = (currentScreen + 1) % 4;
  }

  if (now - lastPublishMs >= PUBLISH_INTERVAL) {
    lastPublishMs = now;
    publishMetrics();
  }
}

void updateSimulatedSensors() {
  hrDrift += (float)(random(-15, 16)) * 0.08f;
  hrDrift  = constrain(hrDrift, -14.0f, 14.0f);
  metrics.heartRate = constrain(72.0f + hrDrift + sin(millis() / 7000.0) * 4.0f, 58.0f, 102.0f);

  metrics.spo2 = constrain(98.0f + (float)(random(-20, 21)) * 0.05f, 95.0f, 100.0f);

  tempDrift += (float)(random(-5, 6)) * 0.02f;
  tempDrift  = constrain(tempDrift, -0.6f, 0.6f);
  metrics.temperature = constrain(36.6f + tempDrift, 35.8f, 37.5f);

  if (random(0, 4) == 0) metrics.steps += random(1, 5);

  static unsigned long lastBat = 0;
  if (millis() - lastBat > 120000) {
    lastBat = millis();
    metrics.battery = max(0, metrics.battery - 1);
  }
}

void checkAlerts() {
  bool prev = metrics.alertActive;
  metrics.alertActive = false;
  String msg = "";

  if (metrics.heartRate > 100)     { metrics.alertActive = true; msg = "Taquicardia detectada!"; }
  if (metrics.heartRate < 50)      { metrics.alertActive = true; msg = "Bradicardia detectada!"; }
  if (metrics.spo2 < 95)           { metrics.alertActive = true; msg = "SpO2 baixo!"; }
  if (metrics.temperature > 37.2f) { metrics.alertActive = true; msg = "Possivel febre"; }

  if (metrics.alertActive && !prev && msg.length() > 0) {
    char alertPayload[128];
    snprintf(alertPayload, sizeof(alertPayload),
      "{\"alerta\":\"%s\",\"device_id\":\"%s\"}",
      msg.c_str(), DEVICE_ID);
    mqtt.publish(TOPIC_ALERT, alertPayload, true);
  }
}

void publishMetrics() {
  if (!mqtt.connected()) return;

  char hr[8], spo[8], tmp[8];
  dtostrf(metrics.heartRate,   1, 0, hr);
  dtostrf(metrics.spo2,        1, 1, spo);
  dtostrf(metrics.temperature, 1, 1, tmp);

  char payload[300];
  snprintf(payload, sizeof(payload),
    "{"
      "\"Metrica\":{"
        "\"Batimentos cardiacos\":\"%s bpm\","
        "\"Saturacao\":\"%s %%\","
        "\"Temperatura\":\"%s C\","
        "\"Passos\":%d"
      "},"
      "\"device_id\":\"%s\","
      "\"battery\":\"%d %%\","
      "\"alerta\":\"%s\""
    "}",
    hr, spo, tmp,
    metrics.steps,
    DEVICE_ID,
    metrics.battery,
    metrics.alertActive ? "SIM" : "NAO"
  );

  bool ok = mqtt.publish(TOPIC_METRICS, payload, false);

  Serial.println(ok ? "\n[MQTT] Publicado OK" : "\n[MQTT] FALHA na publicacao");
  Serial.println("  Metrica");
  Serial.printf ("    Batimentos cardiacos : %s bpm\n", hr);
  Serial.printf ("    Saturacao            : %s %%\n",  spo);
  Serial.printf ("    Temperatura          : %s C\n",   tmp);
  Serial.printf ("    Passos               : %d\n",     metrics.steps);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("CarePlus");

  if (mqtt.connected()) display.fillCircle(120, 4, 3, WHITE);
  else display.drawCircle(120, 4, 3, WHITE);

  drawBattery(95, 0, metrics.battery);
  display.drawLine(0, 10, 127, 10, WHITE);

  switch (currentScreen) {
    case 0: screenHeartRate();   break;
    case 1: screenSpO2();        break;
    case 2: screenTemperature(); break;
    case 3: screenSteps();       break;
  }

  for (int i = 0; i < 4; i++) {
    int cx = 56 + i * 6;
    if (i == currentScreen) display.fillCircle(cx, 62, 2, WHITE);
    else display.drawCircle(cx, 62, 2, WHITE);
  }
  display.display();
}

void screenHeartRate() {
  display.setTextSize(1);
  display.setCursor(24, 14);
  display.print("Freq. Cardiaca");
  display.setTextSize(3);
  display.setCursor(18, 26);
  display.printf("%d", (int)round(metrics.heartRate));
  display.setTextSize(1);
  display.setCursor(90, 40);
  display.print("BPM");
}

void screenSpO2() {
  display.setTextSize(1);
  display.setCursor(30, 14);
  display.print("Saturacao O2");
  display.setTextSize(3);
  display.setCursor(10, 26);
  display.printf("%.1f%%", metrics.spo2);
}

void screenTemperature() {
  display.setTextSize(1);
  display.setCursor(20, 14);
  display.print("Temperatura Corp.");
  display.setTextSize(2);
  display.setCursor(10, 30);
  display.printf("%.1fC", metrics.temperature);
}

void screenSteps() {
  display.setTextSize(1);
  display.setCursor(42, 14);
  display.print("Passos");
  display.setTextSize(2);
  display.setCursor(10, 26);
  display.printf("%d", metrics.steps);
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("MQTT:");
  display.print(mqtt.connected() ? "OK" : "OFF");
  display.setCursor(70, 50);
  display.printf("Bat:%d%%", metrics.battery);
}

void drawBattery(int x, int y, int pct) {
  display.drawRect(x, y + 1, 14, 7, WHITE);
  display.fillRect(x + 14, y + 3, 2, 3, WHITE);
  int w = constrain((pct * 12) / 100, 0, 12);
  display.fillRect(x + 1, y + 2, w, 5, WHITE);
}

void showSplash() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(8, 4);
  display.print("CarePlus");
  display.setTextSize(1);
  display.setCursor(12, 26);
  display.print("Health Tracker v2.0");
  display.setCursor(20, 38);
  display.print("Sprint 2 - IoT");
  display.setCursor(30, 50);
  display.print("Iniciando...");
  display.display();
  delay(2500);
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Conectando a '%s'", WIFI_SSID);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Conectando WiFi...");
  display.display();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("WiFi OK!");
    display.setCursor(0, 14);
    display.print(WiFi.localIP().toString());
    display.display();
    delay(1200);
  } else {
    Serial.println("\n[WiFi] FALHA");
  }
}

void reconnectMQTT() {
  int tries = 0;
  while (!mqtt.connected() && tries < 5) {
    Serial.print("[MQTT] Conectando ao HiveMQ...");
    String cid = String("careplus-esp32-") + String(random(0xFFFF), HEX);

    if (mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println(" conectado!");
      mqtt.publish(TOPIC_STATUS,
        ("{\"device_id\":\"" + String(DEVICE_ID) + "\",\"status\":\"online\"}").c_str(),
        true);
    } else {
      Serial.printf(" falhou rc=%d. Tentando em 3s\n", mqtt.state());
      delay(3000);
      tries++;
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("[MQTT RX] %s -> %s\n", topic, msg.c_str());
}
