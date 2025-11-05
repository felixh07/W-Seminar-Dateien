#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <time.h>

#define I2C_SDA 22
#define I2C_SCL 23
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int bleCount = 0;
const int rssiLimit = -80;
unsigned long lastSave = 0;
const unsigned long SAVE_INTERVAL = 60000; //(in Millisekunden)

const char* ssid = "F"; //WLAN Name einfügen
const char* password = "99999999"; //WLAN Passwort einfügen

WiFiServer server(80);
bool serverRunning = false;
bool timeSynced = false;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    int rssi = advertisedDevice.getRSSI();
    if (rssi >= rssiLimit) {
      bleCount++;
      
    
    }
  }
};

MyAdvertisedDeviceCallbacks bleCb; 

String getTimestamp() {
  time_t now = time(nullptr);
  if (now > 100000 && timeSynced) { 
    time_t t2 = now + 2 * 3600; 
    struct tm timeinfo;
    localtime_r(&t2, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
  } else {

    unsigned long seconds = millis() / 1000;
    return String(seconds/60) + "min";
  }
}

void logToCSV(int count) {
  File file = SPIFFS.open("/log.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Fehler beim Öffnen der Datei!");
    return;
  }

  String timestamp = getTimestamp();
  file.print(timestamp);
  file.print(",");
  file.println(count);
  file.close();

  Serial.println("Wert gespeichert: " + timestamp + "," + String(count));
}

void startWifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi connect started (non-blocking)...");
}

void trySetupNetworkServices() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeSynced) {
      Serial.println();
      Serial.println("WLAN verbunden, versuche NTP-Sync...");
      
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
     
      unsigned long tstart = millis();
      struct tm timeinfo;
      while (!getLocalTime(&timeinfo) && (millis() - tstart < 10000)) {
        delay(200);
      }
      if (getLocalTime(&timeinfo)) {
        timeSynced = true;
        Serial.println("NTP synchronisiert.");
      } else {
        Serial.println("NTP-Sync fehlgeschlagen (Timeout). Weiter mit Fallback-Zeit.");
        timeSynced = false;
      }
    }
    if (!serverRunning) {
      server.begin();
      serverRunning = true;
      Serial.println("HTTP-Server gestartet.");
    }
  } else {
    if (serverRunning) {
      server.stop();
      serverRunning = false;
      Serial.println("WLAN getrennt -> Server gestoppt.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED nicht gefunden!");
    while (true) delay(1000);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("BLE Logger startet...");
  display.display();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS konnte nicht gestartet werden!");
    while(true);
  }

  {
    File f = SPIFFS.open("/log.csv", FILE_WRITE);
    if (f) {
      f.println("Zeit,Zaehler");
      f.close();
      Serial.println("Neue log.csv gestartet!");
    } else {
      Serial.println("Fehler: log.csv konnte nicht erstellt werden!");
    }
  }

  BLEDevice::init("ESP32-BLE-Scanner");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleCb);
  pBLEScan->setActiveScan(true);

  startWifiConnect();

  lastSave = millis();
}

void handleClientRequests() {
  if (!serverRunning) return;
  WiFiClient client = server.available();
  if (!client) return;
  Serial.println("Neuer Client verbunden");
  unsigned long reqStart = millis();
  while (!client.available() && (millis() - reqStart < 5000)) {
    delay(1);
  }
  if (!client.available()) {
    client.stop();
    Serial.println("Client Timeout - abgebrochen");
    return;
  }

  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("GET /log.csv") != -1) {
    File file2 = SPIFFS.open("/log.csv", FILE_READ);
    if (file2) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/csv");
      client.println("Connection: close");
      client.println();
      while (file2.available()) {
        client.write(file2.read());
      }
      file2.close();
    } else {
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: text/plain");
      client.println("Connection: close");
      client.println();
      client.println("log.csv nicht gefunden");
    }
  } else {
    // Startseite
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<h1>ESP32 BLE Logger</h1>");
    client.println("<p><a href=\"/log.csv\">log.csv herunterladen</a></p>");
  }
  delay(1);
  client.stop();
  Serial.println("Client getrennt");
}

void loop() {
  bleCount = 0;
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleCb);
  pBLEScan->start(5, false); 

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("BLE: ");
  display.println(bleCount);
  display.display();

  Serial.printf("Gesamt BLE-Geräte (≥ %d dBm): %d\n", rssiLimit, bleCount);

  if (millis() - lastSave >= SAVE_INTERVAL) {
    logToCSV(bleCount);
    lastSave = millis();
  }

  static unsigned long lastNetCheck = 0;
  if (millis() - lastNetCheck > 5000) { // alle 5s prüfen
    lastNetCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect(); 
    }
    trySetupNetworkServices();
  }

  if (serverRunning) handleClientRequests();

  delay(100);
}
