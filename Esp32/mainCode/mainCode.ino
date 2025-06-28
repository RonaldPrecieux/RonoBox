#include <WiFi.h>
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
ConfigManager configManager;
// Définition des broches
#define DHT_PIN 4         // Broche digitale pour DHT11
#define WATER_LEVEL_PIN 34 // Broche analogique pour le capteur de niveau d'eau
#define SOIL_MOISTURE_PIN 35 // Broche analogique pour l'humidité du sol
#define PIR_PIN 18        // Broche digitale pour le capteur de présence
#define MQ2_PIN 32        // Broche analogique pour MQ2

#include <DHT.h>
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

class MySmartHomeDevice : public MQTTDevice {
public:
    // Utilisation de WiFi.macAddress() pour ESP32
    MySmartHomeDevice() : MQTTDevice(getMacAddress()) {}

    String getMacAddress() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18] = {0};
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }

    void setupHA() {
        // Envoyer les configurations avec gestion des erreurs
        //bool HADiscoveryConfig::sendSensorConfig(const String & location, const String & sensor, const String & deviceClass, const String & unit, const String & friendlyName)
        //Capteur de temperature
        if(!getHAConfig().sendSensorConfig("salon", "temperature", "temperature", "°C", "Température Salon")) {
            Serial.println("Échec de la configuration du capteur de température");
        }else{
        Serial.println("Succès de la configuration du capteur de température");

        }
        
        delay(500); // Délai entre les configurations
        //Capteur d'humidité
        if(!getHAConfig().sendSensorConfig("salon", "humidite", "humidity", "%", "Humidité Salon")) {
            Serial.println("Échec de la configuration du capteur d'humidité");
        }else{
        Serial.println("Succès de la configuration du capteur d'humidité");

        }
        
        delay(500);
        //Switch Lampe
        if(!getHAConfig().sendSwitchConfig("cuisine", "lampe", "Lampe Cuisine")) {
            Serial.println("Échec de la configuration de la lampe");
        } else{
                  Serial.println("Succès de la configuration de la lampe");

        }

                // Capteur de gaz MQ2
        if (!getHAConfig().sendSensorConfig("salon", "gaz", "gas", "ppm", "Détection de gaz (MQ2)")) {
            Serial.println("Échec de la configuration du capteur de gaz");
        } else {
            Serial.println("Succès de la configuration du capteur de gaz");
        }
        delay(500);

        // Capteur d’humidité du sol
        if (!getHAConfig().sendSensorConfig("salon", "humidite_sol", "moisture", "%", "Humidité du sol")) {
            Serial.println("Échec de la configuration du capteur d'humidité du sol");
        } else {
            Serial.println("Succès de la configuration du capteur d'humidité du sol");
        }
        delay(500);

        // Capteur de niveau d’eau
        if (!getHAConfig().sendSensorConfig("salon", "niveau_eau", "moisture", "%", "Niveau d’eau")) {
            Serial.println("Échec de la configuration du capteur de niveau d’eau");
        } else {
            Serial.println("Succès de la configuration du capteur de niveau d’eau");
        }
        delay(500);

        // Capteur PIR (détection de mouvement) - binaire
        if (!getHAConfig().sendBinarySensorConfig("salon", "presence", "motion", "Présence détectée")) {
            Serial.println("Échec de la configuration du capteur PIR");
        } else {
            Serial.println("Succès de la configuration du capteur PIR");
        }

    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "lampe") {
            // Contrôler la lampe
            Serial.print("Commande lampe reçue: ");
            Serial.println(value);
            publishSensorData(location, device, value); // Mettre à jour l'état
            Serial.printf("Commande Lampe reçue et exécutée %s dans la %s\n", value.c_str(), location.c_str());
        }
        // ... autres commandes
    }
};

MySmartHomeDevice device;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("App Launching");
  if (!configManager.begin()) {
    Serial.println("Mode configuration AP actif");
    Serial.println("Connectez-vous au WiFi 'SmartHome-Config'");
    Serial.println("Ouvrez http://192.168.4.1 dans votre navigateur");
    while (!configManager.isConfigured()) {
            configManager.handleClient();
            delay(100);
        }
 } // Bloquer en mode configuration

    // Récupère la config Debug
    NetworkConfig config = configManager.getConfig();
      Serial.print("Tentative de connexion à: ");
        Serial.println(config.wifiSSID);
        Serial.print("Avec le mot de passe: ");
        Serial.println(config.wifiPassword); // Attention: ne faites pas ça en production!
            
   

  // Utilisation de begin() avec la configuration chargée
  device.begin(config.mqttServer.c_str());

    // Attendre que la connexion soit stable
    delay(2000);
    
    // Configuration des appareils Home Assistant
    device.setupHA();
    dht.begin();
    pinMode(PIR_PIN, INPUT);
}
void loop() {
     configManager.handleClient(); // Gère le serveur web si en mode AP
  
  if (configManager.isConfigured()) {
     // Gérer la connexion MQTT
    device.handle();
    
    // === Lecture des capteurs ===

    // Température et humidité (DHT11)
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    delay(200);

    // Niveau d'eau (analogique -> pourcentage)
    int waterLevel = analogRead(WATER_LEVEL_PIN);
    int waterLevelPercentage = map(waterLevel, 0, 4095, 0, 100);  // pour ESP32
    delay(200);

    // Humidité du sol
    int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
    int soilMoisturePercentage = map(soilMoisture, 4095, 0, 0, 100); // inversé car sec = + élevé
    delay(200);

    // Présence (PIR)
    int presence = digitalRead(PIR_PIN);
    delay(200);

    // Gaz MQ2
    int gasValue = analogRead(MQ2_PIN);
    int gasPercentage = map(gasValue, 0, 4095, 0, 100); // estimation
    delay(200);

    // === Envoi toutes les 1 secondes ===
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 1000) {
        // Température et humidité
        device.publishSensorData("salon", "temperature", temperature);
        device.publishSensorData("salon", "humidite", humidity);

        // Niveau d'eau
        device.publishSensorData("salon", "niveau_eau", waterLevelPercentage);

        // Humidité du sol
        device.publishSensorData("salon", "humidite_sol", soilMoisturePercentage);

        // Gaz (brut ou pourcentage estimé)
        device.publishSensorData("salon", "gaz", gasPercentage);

        // Présence (binary_sensor → booléen)
        device.publishSensorData("salon", "presence", presence);

        lastSend = millis();
    }
  }
   
   
}
