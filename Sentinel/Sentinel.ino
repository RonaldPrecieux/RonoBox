#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>


ConfigManager configManager;

// Définition des broches
#define BUZZER_PIN 34
#define GAS_PIN 35
#define PRESENCE_PIN 32
#define TEMP_PIN 33
#define LCD_SDA 21
#define LCD_SCL 22
#define DHT_PIN 33  // Broche connectée au DHT11 (remplace TEMP_PIN si besoin)
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastWifiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
const unsigned long retryInterval = 15000;

bool wifiConnected = false;
bool mqttConnected = false;

// Initialisation LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adresse I2C 0x27, écran 16x2

class KitchenDevice : public MQTTDevice {
public:
    KitchenDevice() : MQTTDevice(getMacAddress()) {}

    String getMacAddress() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18] = {0};
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }

    void setupHA() {
        // Capteur de température
        if(!getHAConfig().sendSensorConfig("cuisine", "temperature", "temperature", "°C", "Température Cuisine")) {
            Serial.println("Échec configuration température");
        }
        
        // Capteur de gaz
        if(!getHAConfig().sendSensorConfig("cuisine", "gaz", "gas", "ppm", "Détection de gaz")) {
            Serial.println("Échec configuration capteur gaz");
        }
        
        // Capteur de présence
        if(!getHAConfig().sendBinarySensorConfig("cuisine", "presence", "motion", "Présence Cuisine")) {
            Serial.println("Échec configuration capteur présence");
        }
        
        // Buzzer comme un switch
        if(!getHAConfig().sendSwitchConfig("cuisine", "buzzer", "Alarme Cuisine")) {
            Serial.println("Échec configuration buzzer");
        }
    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "buzzer") {
            digitalWrite(BUZZER_PIN, value == "ON" ? HIGH : LOW);
            publishSensorData(location, device, value);
            updateLCD();
        }
    }

   void updateLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(readTemperature());
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(readHumidity());
    lcd.print(" %");
}

   float readTemperature() {
    float temp = dht.readTemperature();  // °C
    if (isnan(temp)) {
        Serial.println("Erreur lecture température");
        return -1;
    }
    return temp;
}

float readHumidity() {
    float hum = dht.readHumidity();
    if (isnan(hum)) {
        Serial.println("Erreur lecture humidité");
        return -1;
    }
    return hum;
}


    int readGasLevel() {
        return analogRead(GAS_PIN);
    }

    bool readPresence() {
        return digitalRead(PRESENCE_PIN);
    }


    

};
void tryReconnectWiFi(NetworkConfig config) {
    if (!wifiConnected && millis() - lastWifiAttempt > retryInterval) {
        Serial.println("Tentative de reconnexion WiFi...");
        WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
        lastWifiAttempt = millis();
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("✅ WiFi reconnecté !");
    }
  }
KitchenDevice device;

void setup() {
    Serial.begin(115200);
    dht.begin();
    // Initialisation des broches
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GAS_PIN, INPUT);
    pinMode(PRESENCE_PIN, INPUT);
    pinMode(DHT_PIN, INPUT);
    
    // Initialisation LCD
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();
    lcd.print("Initialisation...");

    // Configuration WiFi/MQTT
    if (!configManager.begin()) {
        lcd.clear();
        lcd.print("Mode config AP");
        lcd.setCursor(0, 1);
        lcd.print("192.168.4.1");
    }
    // if (!configManager.begin()) {
    //     lcd.clear();
    //     lcd.print("Mode config AP");
    //     lcd.setCursor(0, 1);
    //     lcd.print("192.168.4.1");
        
    //     while (!configManager.isConfigured()) {
    //         configManager.handleClient();
    //         delay(100);
    //     }
    // }

  NetworkConfig config = configManager.getConfig();
    // WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
    
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
    // }

    mqttConnected = device.begin(config.mqttServer.c_str(), config.mqttPort);
if (mqttConnected) {
    device.setupHA();
    lcd.clear();
    device.updateLCD();
} else {
    Serial.println("MQTT non disponible, tentative plus tard...");
    lcd.clear();
    lcd.print("MQTT indispo");
}

}

void loop() {
    static unsigned long lastUpdate = 0;
    static NetworkConfig config = configManager.getConfig();

    configManager.handleClient();  // Pour le portail

if(!wifiConnected) tryReconnectWiFi(config);
    device.handle();

   // tryReconnectMQTT(config);

    if (millis() - lastUpdate > 2000) { // Toutes les 2 secondes
        // Lecture des capteurs
        float temperature = device.readTemperature();
        int gasLevel = device.readGasLevel();
        bool presence = device.readPresence();

        // Envoi des données
        device.publishSensorData("cuisine", "temperature", temperature);
        device.publishSensorData("cuisine", "gaz", gasLevel);
        device.publishSensorData("cuisine", "presence", presence ? "ON" : "OFF");

        // Gestion alarme gaz
        if (gasLevel > 50) { // Seuil à ajuster
            digitalWrite(BUZZER_PIN, HIGH);
            device.publishSensorData("cuisine", "buzzer", "ON");
        } else {
            digitalWrite(BUZZER_PIN, LOW);
        }

        // Mise à jour LCD
        device.updateLCD();
        
        lastUpdate = millis();
    }
}