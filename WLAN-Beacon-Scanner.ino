#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define I2C_SDA 22
#define I2C_SCL 23
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RSSI_Grenze -80

void setup() {
    Serial.begin(115200);

    Wire.begin(I2C_SDA, I2C_SCL);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  
}

void loop() {
    int deviceCount = 0;
    int n = WiFi.scanNetworks(false, true); 

    for (int i = 0; i < n; ++i) {
        int rssi = WiFi.RSSI(i); 
        if (rssi > RSSI_Grenze) { 
            deviceCount++;
        }
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 20);
    display.print("Pax: ");
    display.println(deviceCount);
    display.display();

    delay(5000); 
}

