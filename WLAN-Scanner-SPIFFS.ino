#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>

extern "C" {
  #include "esp_wifi.h"
  #include "esp_wifi_types.h"
}

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA 22
#define I2C_SCL 23
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;



#define WIFI_SSID    "WLAN-Name" //WLAN Name einfügen
#define WIFI_PASS    "WLAN-Passwort" //WLAN Passwort einfügen


#define MAX_DEVICES 200
uint8_t knownDevices[MAX_DEVICES][6];
int knownDeviceCount = 0;
int deviceCount = 0;

const int rssiLimit = -80;

unsigned long lastSave = 0;
const unsigned long SAVE_INTERVAL = 10000; //(in Millisekunden)

WiFiServer server(80);
bool serverRunning = false;
bool timeSynced = false;

bool isKnown(uint8_t *mac) {
  for (int i = 0; i < knownDeviceCount; i++) {
    if (memcmp(knownDevices[i], mac, 6) == 0) return true;
  }
  return false;
}

void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  int rssi = pkt->rx_ctrl.rssi;
  uint8_t* mac = pkt->payload + 10;

  if (rssi >= rssiLimit && !isKnown(mac) && knownDeviceCount < MAX_DEVICES) {
    memcpy(knownDevices[knownDeviceCount], mac, 6);
    knownDeviceCount++;
    deviceCount++;
  }
}

void resetDevices() {
  knownDeviceCount = 0;
  deviceCount = 0;
  memset(knownDevices, 0, sizeof(knownDevices));
}

void showPaxCount(int count) {
  if (!oledAvailable) return;
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.setTextColor(WHITE);
  display.print("Pax: ");
  display.println(count);
  display.display();
}

bool getDateTimeString(char *outBuf, size_t bufLen) {
  time_t now;
  struct tm timeinfo;
  if (time(&now) == (time_t)0) return false;

  now += 2 * 3600; 
  if (!localtime_r(&now, &timeinfo)) return false;
  if (timeinfo.tm_year + 1900 < 2020) return false;

  strftime(outBuf, bufLen, "%Y-%m-%d %H:%M:%S", &timeinfo);
  return true;
}

void logToCSV(int count) {
  File file = SPIFFS.open("/log.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Fehler beim Öffnen der Datei!");
    return;
  }

  char timestr[32];
  if (getDateTimeString(timestr, sizeof(timestr))) {
    file.print(timestr);
    file.print(",");
    Serial.print("Wert gespeichert: ");
    Serial.print(timestr);
  } else {
    unsigned long timestamp = millis() / 1000;
    String s = String(timestamp) + "s";
    file.print(s);
    file.print(",");
    Serial.print("Wert gespeichert (fallback): ");
    Serial.print(s);
  }

  file.println(count);
  file.close();

  Serial.println("," + String(count));
}

void tryConnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("WLAN-Verbindung wird versucht...");
  }
}

void checkNetwork() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeSynced) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      struct tm ti;
      if (getLocalTime(&ti)) {
        timeSynced = true;
        Serial.println("NTP synchronisiert.");
      }
    }
    if (!serverRunning) {
      server.begin();
      serverRunning = true;
      Serial.println("HTTP-Server gestartet.");
      Serial.print("http://");
      Serial.print(WiFi.localIP());
      Serial.println("/log.csv");
    }
  } else {
    if (serverRunning) {
      server.stop();
      serverRunning = false;
      Serial.println("WLAN getrennt, Server gestoppt.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledAvailable = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("WLAN-Scanner startet...");
    display.display();
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS konnte nicht gestartet werden!");
    while (true);
  }

  File file = SPIFFS.open("/log.csv", FILE_WRITE);
  if (file) {
    file.println("Zeit,Zaehler");
    file.close();
    Serial.println("Neue log.csv gestartet!");
  }

  WiFi.mode(WIFI_MODE_STA);
  tryConnectWiFi(); 
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
}

void loop() {
  showPaxCount(deviceCount);

  if (millis() - lastSave > SAVE_INTERVAL) {
    logToCSV(deviceCount);
    lastSave = millis();
    resetDevices();
  }

  static unsigned long lastNetCheck = 0;
  if (millis() - lastNetCheck > 5000) {
    lastNetCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      tryConnectWiFi();
    }
    checkNetwork();
  }

  if (serverRunning) {
    WiFiClient client = server.available();
    if (client) {
      unsigned long t0 = millis();
      while (!client.available() && (millis() - t0) < 2000) delay(1);

      if (client.available()) {
        String request = client.readStringUntil('\r');
        client.flush();

        if (request.indexOf("GET /log.csv") != -1) {
          File f = SPIFFS.open("/log.csv", FILE_READ);
          if (f) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/csv");
            client.println("Content-Disposition: attachment; filename=\"log.csv\"");
            client.println("Connection: close");
            client.println();
            while (f.available()) client.write(f.read());
            f.close();
          }
        } else {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<h1>ESP32 WLAN-Scanner</h1>");
          client.println("<p><a href=\"/log.csv\">Log herunterladen</a></p>");
        }
        delay(1);
        client.stop();
      }
    }
  }

  delay(50);
}
