#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

#define BUTTON_PIN 5
#define SAVE_INTERVAL 20000 


const char* ssid = "WLAN-Name"; //WLAN Name einfügen
const char* password = "WLAN-Passwort"; //WLAN Passwort einfügen


unsigned long lastSave = 0;
int buttonCount = 0;
int lastButtonState = HIGH;

WiFiServer server(80);

void logToCSV(int count) {
  File file = SPIFFS.open("/log.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Fehler beim Öffnen der Datei!");
    return;
  }
  unsigned long seconds = millis() / 1000;
  file.print(seconds);
  file.print("s,");
  file.println(count);
  file.close();
  Serial.println("Wert gespeichert: " + String(seconds) + "s," + String(count));
}

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS konnte nicht gestartet werden!");
    while (true);
  }

  File file = SPIFFS.open("/log.csv", FILE_WRITE);
  if (file) {
    file.println("Zeit,Zaehler");
    file.close();
    Serial.println("Neue log.csv gestartet!");
  } else {
    Serial.println("Fehler beim Anlegen der log.csv!");
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden! IP-Adresse: " + WiFi.localIP().toString());

  server.begin();
}

void loop() {
  int state = digitalRead(BUTTON_PIN);
  if (state == LOW && lastButtonState == HIGH) {
    buttonCount++;
    Serial.println("Knopf gedrueckt, Zaehler = " + String(buttonCount));
    delay(50); 
  }
  lastButtonState = state;

  if (millis() - lastSave > SAVE_INTERVAL) {
    logToCSV(buttonCount);
    lastSave = millis();
  }

  WiFiClient client = server.available();
  if (client) {
    Serial.println("Neuer Client verbunden");
    while(!client.available()){
      delay(1);
    }
    String request = client.readStringUntil('\r');
    client.flush();

    if (request.indexOf("GET /log.csv") != -1) {
      File file = SPIFFS.open("/log.csv", FILE_READ);
      if (file) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/csv");
        client.println("Connection: close");
        client.println();
        while(file.available()){
          client.write(file.read());
        }
        file.close();
      }
    } else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<h1>ESP32 Knopfzaehler</h1>");
      client.println("<p><a href=\"/log.csv\">log.csv herunterladen</a></p>");
    }
    delay(1);
    client.stop();
    Serial.println("Client getrennt");
  }
}
