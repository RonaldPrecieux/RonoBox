#ifdef MODULE_SMARTPLUG

#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"

#define DEBUG 1

ConfigManager configManager;

// Définition de la broche du relais
#define RELAY_PIN 4 // À adapter selon votre hardware
#define LED_PIN  5

// Variables pour le clignotement
unsigned long BlinkpreviousMillis = 0;
const long fastBlinkInterval = 200;  // Intervalle rapide (non connecté WiFi)
const long normalBlinkInterval = 500; // Intervalle normal (WiFi OK mais pas MQTT)
bool ledState = LOW;


IPAddress MQTTBrokerip;

unsigned long lastWifiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
const unsigned long retryInterval = 15000;

bool wifiConnected = false;
bool mqttConnected = false;

class SmartPlugDevice : public MQTTDevice {
public:
    SmartPlugDevice() : MQTTDevice(getMacAddress()) {}

    String getMacAddress() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18] = {0};
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }

    void setupHA() {
        // Configuration de la prise dans Home Assistant
        if(!getHAConfig().sendSwitchConfig("salon", "prise", "Prise Connectée")) {
            Serial.println("Échec configuration prise");
        }
        
        // Publier l'état initial ON (relais NC fermé)
        publishSensorData("salon", "prise", "ON");
    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "prise") {
            // Logique inversée pour relais NC
            // ON = relais NON activé (circuit fermé)
            // OFF = relais activé (circuit ouvert)
            digitalWrite(RELAY_PIN, value == "ON" ? LOW : HIGH);
            Serial.println("Commande reçue pour la prise: " + value);
            publishSensorData(location, device, value);
        }
    }

    void updateLED() {
        unsigned long currentMillis = millis();
        
        if (!wifiConnected) {
            // Clignotement rapide - Pas de WiFi
            Serial.println("No wifi");
            if (currentMillis - BlinkpreviousMillis >= fastBlinkInterval) {
                BlinkpreviousMillis = currentMillis;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);                                                    
            
        }    
        }else if (!mqttConnection) {
                        Serial.println("No MQTT");

            // Clignotement normal - WiFi OK mais pas MQTT
            if (currentMillis - BlinkpreviousMillis >= normalBlinkInterval) {
                BlinkpreviousMillis = currentMillis;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);
            }
        } else {
            // LED allumée en continu - Tout est connecté
            Serial.println("Connected to MQTT led ON");
            digitalWrite(LED_PIN, HIGH);
        }
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

SmartPlugDevice device;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // LED éteinte au démarrage    
    // Initialisation de la broche du relais
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // LOW = relais non activé (circuit fermé pour NC)
    
    // Configuration WiFi/MQTT
    if (!configManager.begin()) {
        Serial.println("Mode configuration AP activé");
    }

    if (WiFi.hostByName("raspberrypi.local", MQTTBrokerip)) {
        Serial.print("IP du broker MQTT: ");
        Serial.println(MQTTBrokerip);
    } else {
        Serial.println("Échec de résolution DNS");
    }
    
    bool mqttConnected = device.begin(MQTTBrokerip);

    if (mqttConnected) {
        Serial.println("Connecté au broker MQTT!");
        device.setupHA();
    } else {
        Serial.println("MQTT non disponible, tentative plus tard...");
    }
}

void loop() {
    static NetworkConfig config = configManager.getConfig();

    configManager.handleClient();  // Pour le portail de configuration

    if(!wifiConnected) tryReconnectWiFi(config);

    device.handle();
    device.updateLED();  // Met à jour l'état de la LED
}
#endif