#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

#define MAX_DEVICES 200
uint8_t knownDevices[MAX_DEVICES][6];
int knownDeviceCount = 0;
int deviceCount = 0;

const int rssiLimit = -80;
unsigned long scanStart = 0;

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
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.setTextColor(WHITE);
  display.print("Pax: ");
  display.println(count);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED nicht gefunden"));
    while (true);
  }

  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);

  scanStart = millis();
}

void loop() {

  showPaxCount(deviceCount);

  delay(100);
}
