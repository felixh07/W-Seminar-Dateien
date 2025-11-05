#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define I2C_SDA 22
#define I2C_SCL 23
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int bleCount = 0;
const int rssiLimit = -90;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    int rssi = advertisedDevice.getRSSI();
    if (rssi >= rssiLimit) {
      bleCount++;
      Serial.printf("BLE-Gerät: %s | RSSI: %d\n",
                    advertisedDevice.getAddress().toString().c_str(),
                    rssi);
    }
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED nicht gefunden!");
    while (true);
  }

  BLEDevice::init("ESP32-BLE-Scanner");
}

void loop() {
  bleCount = 0;

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(1, false); 

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.print("Pax: ");
  display.println(bleCount);
  display.display();

  Serial.printf("Gesamt BLE-Geräte (≥ %d dBm): %d\n\n", rssiLimit, bleCount);
  delay(500);
}
