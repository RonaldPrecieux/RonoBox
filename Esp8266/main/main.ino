#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
#define BOUTON_RESET_CONFIG 0

class MySmartHomeDevice : public MQTTDevice {
public:
    // Utilisation de WiFi.macAddress() pour ESP8266
    MySmartHomeDevice() : MQTTDevice(WiFi.macAddress()) {}

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
            Serial.printf("Commande Lampe recu et exeuté %s dans la ",location);
        }
        // ... autres commandes
    }
};
ConfigManager configManager;
MySmartHomeDevice device;

void setup() {

  Serial.begin(115200);
  delay(2000);


  pinMode(BOUTON_RESET_CONFIG, INPUT_PULLUP);

 int holdTime = 0;
while (digitalRead(BOUTON_RESET_CONFIG) == LOW) {
  delay(100);
  holdTime += 100;
  if (holdTime > 3000) break; // Appui de 3 secondes
}

if (holdTime > 3000) {
  configManager.resetConfiguration();
  ESP.restart();
}
  Serial.println("App Launching");
  // Récupère la config pour Debug
              
  if (!configManager.begin()) {
    Serial.println("Mode configuration AP actif");
    Serial.println("Connectez-vous au WiFi 'SmartHome-Config'");
    Serial.println("Ouvrez http://192.168.4.1 dans votre navigateur");
    Serial.println("##############Debug##################");
      NetworkConfig config = configManager.getConfig();

      Serial.println("Tentative de connexion à: ");
        Serial.println(config.wifiSSID);
        Serial.print("Avec le mot de passe: ");
        Serial.println(config.wifiPassword); // Attention: ne faites pas ça en production!
            
    while (!configManager.isConfigured()) {
            configManager.handleClient();
            delay(100);
        }
 } // Bloquer en mode configuration

      NetworkConfig config = configManager.getConfig();


  // Utilisation de begin() avec la configuration chargée
    device.begin(config.mqttServer.c_str());

    // Attendre que la connexion soit stable
    delay(2000);


    device.setupHA();
      // Envoyer les configurations avec gestion des erreurs
    
    delay(500); // Délai entre les configurations
    
}

void loop() {

    // Utilisation de handle() au lieu de loop()
    device.handle();
    
    // Simuler des données de capteurs
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 5000) {
        float temperature = random(200, 300) / 10.0;
        float humidity = random(300, 800) / 10.0;
         // Température et humidité
        device.publishSensorData("salon", "temperature", temperature);
        device.publishSensorData("salon", "humidite", humidity);
      
        lastSend = millis();
    }
}