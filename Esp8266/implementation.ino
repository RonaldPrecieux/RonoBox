#include "MQTTDevice.h"

class MySmartHomeDevice : public MQTTDevice {
public:
    MySmartHomeDevice() : MQTTDevice("AABBCCDDEEFF") {}

    void setupHA() {
        // Configurer les capteurs
        getHAConfig().sendSensorConfig("salon", "temperature", "temperature", "°C", "Température Salon");
        getHAConfig().sendSensorConfig("salon", "humidite", "humidity", "%", "Humidité Salon");
        
        // Configurer les actionneurs
        getHAConfig().sendSwitchConfig("salon", "lampe", "Lampe Salon");
        getHAConfig().sendSwitchConfig("salon", "prise_tele", "Prise Télé");
    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "lampe") {
            // Contrôler la lampe
            publishSensorData(location, device, value); // Mettre à jour l'état
        }
        // ... autres commandes
    }
};

MySmartHomeDevice device;

void setup() {
    device.setup("monWiFi", "monMotDePasse", "192.168.1.100");
    device.setupHA();
}

void loop() {
    device.loop();
    
    // Simuler des données de capteurs
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 5000) {
        float temp = random(200, 300) / 10.0;
        device.publishSensorData("salon", "temperature", String(temp));
        lastSend = millis();
    }
}