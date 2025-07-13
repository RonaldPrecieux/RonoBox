#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
#define BOUTON_RESET_CONFIG 0

// Définitions des broches
#define RELAY_PIN D5      // Broche pour le relais
#define SOUND_SENSOR_PIN D6  // Broche analogique pour le capteur de son

class MySmartHomeDevice : public MQTTDevice {
private:
    bool lampState;
    bool soundControlEnabled;
    unsigned long lastSoundDetection;
    const unsigned long soundTimeout = 1000;
 
public:
    MySmartHomeDevice() : MQTTDevice(WiFi.macAddress()), lampState(false), soundControlEnabled(true), lastSoundDetection(0) {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW);
        pinMode(SOUND_SENSOR_PIN, INPUT);
    }

        void initPublish(){
        publishSensorData("salon", "lampe", lampState ? "ON" : "OFF");
        // Control Sound
        publishSensorData("salon", "sound", soundControlEnabled ? "ON" : "OFF");


    }

    void setupHA() {    
        // Switch Lampe
        if(!getHAConfig().sendSwitchConfig("salon", "lampe", "Lampe Salon")) {
            Serial.println("Échec de la configuration de la lampe");
        } else {
            Serial.println("Succès de la configuration de la lampe");
        }

        delay(500);
        
        // Capteur de son
        //    bool sendBinarySensorConfig(const String& location, const String& sensor,
       //                        const String& deviceClass, const String& friendlyName);

        if(!getHAConfig().sendBinarySensorConfig("salon", "detection_son", "song", "Détection de son")) {
            Serial.println("Échec de la configuration du capteur de son");
        } else {
            Serial.println("Succès de la configuration du capteur de son");
        }

        delay(500);
        
        // Switch pour contrôle par son
        if(!getHAConfig().sendSwitchConfig("salon", "sound", "Controle veilleuse par son")) {
            Serial.println("Échec de la configuration du contrôle par son");
        } else {
            Serial.println("Succès de la configuration du contrôle par son");
        }
    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        Serial.printf("Commande reçue - Location: %s, Device: %s, Value: %s\n", 
                location.c_str(), device.c_str(), value.c_str());
    
        if (device == "lampe") {
            setLampState(value == "ON");
            Serial.print("Commande lampe reçue: ");
            Serial.println(value);
            publishSensorData(location, device, lampState ? "ON" : "OFF");
        }
        else if (device == "sound") {
            soundControlEnabled = (value == "ON");
            publishSensorData(location, device, soundControlEnabled ? "ON" : "OFF");
            Serial.print("Contrôle par son ");
            Serial.println(soundControlEnabled ? "activé" : "désactivé");
        }
    }



    void setLampState(bool state) {
        lampState = state;
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
        publishSensorData("salon", "lampe", state ? "ON" : "OFF");
    }

    void toggleLamp() {
        setLampState(!lampState);
    }

    void checkSoundSensor() {
        if (!soundControlEnabled) return;
        
        int soundValue = digitalRead(SOUND_SENSOR_PIN);
        
        if (soundValue && (millis() - lastSoundDetection > soundTimeout)) {
            lastSoundDetection = millis();
            toggleLamp();
            publishSensorData("salon", "detection_son", "ON");
        }
    }

    void handle() {
        MQTTDevice::handle();
        checkSoundSensor();
    }
};

ConfigManager configManager;
MySmartHomeDevice device;
NetworkConfig config;

void setup() {
    Serial.begin(115200);
    delay(2000);


    Serial.println("App Launching");
              
    if (!configManager.begin()) {
        Serial.println("Mode configuration AP actif");
        Serial.println("Connectez-vous au WiFi 'SmartHome-Config'");
        Serial.println("Ouvrez http://192.168.4.1 dans votre navigateur");
        
        // Debug
        NetworkConfig config = configManager.getConfig();
        Serial.println("Tentative de connexion à: ");
        Serial.println(config.wifiSSID);
            
        while (!configManager.isConfigured()) {
            configManager.handleClient();
            delay(100);
        }
    }

    config = configManager.getConfig();
    device.begin(config.mqttServer.c_str());
    delay(2000);
    device.setupHA();
    delay(500);
    device.initPublish();
}


void loop() {
 
    device.handle();
    
    
    // Simuler des données de capteurs
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 5000) {
        float temperature = random(200, 300) / 10.0;
        float humidity = random(300, 800) / 10.0;
        
        device.publishSensorData("salon", "temperature", temperature);
        device.publishSensorData("salon", "humidite", humidity);
      
        lastSend = millis();
    }
}