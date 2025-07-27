#ifdef MODULE_VEILLEUSE

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
#define BOUTON_RESET_CONFIG 0

// Définitions des broches
#define RELAY_PIN D5      // Broche pour le relais
#define SOUND_SENSOR_PIN D6  // Broche analogique pour le capteur de son

unsigned long lastWifiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
const unsigned long retryInterval = 15000;

#define LED_PIN  4

// Variables pour le clignotement
unsigned long BlinkpreviousMillis = 0;
const long fastBlinkInterval = 200;  // Intervalle rapide (non connecté WiFi)
const long normalBlinkInterval = 500; // Intervalle normal (WiFi OK mais pas MQTT)
bool ledState = LOW;

bool wifiConnected = false;
bool mqttConnected = false;
IPAddress MQTTBrokerip;

ConfigManager configManager;
NetworkConfig config;

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
        publishSensorData("salon", "lampe", lampState ? String("ON") : String("OFF"));
        // Control Sound
        publishSensorData("salon", "sound", soundControlEnabled ? String("ON") : String("OFF"));


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
            publishSensorData(location, device, lampState ? String("ON") : String("OFF"));
        }
        else if (device == "sound") {
            soundControlEnabled = (value == "ON");
            publishSensorData(location, device, soundControlEnabled ? String("ON") : String("OFF"));
            Serial.print("Contrôle par son ");
            Serial.println(soundControlEnabled ? "activé" : "désactivé");
        }
    }



    void setLampState(bool state) {
        lampState = state;
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
        publishSensorData("salon", "lampe", state ? String("ON") : String("OFF"));
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

    void updateLED() {
        unsigned long currentMillis = millis();
        
        if (!wifiConnected) {
            // Clignotement rapide - Pas de WiFi
            if (currentMillis - BlinkpreviousMillis >= fastBlinkInterval) {
                BlinkpreviousMillis = currentMillis;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);
            }
        } else if (!mqttConnection) {
            // Clignotement normal - WiFi OK mais pas MQTT
            if (currentMillis - BlinkpreviousMillis >= normalBlinkInterval) {
                BlinkpreviousMillis = currentMillis;
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState);
            }
        } else {
            // LED allumée en continu - Tout est connecté
            digitalWrite(LED_PIN, HIGH);
        }
    }


    void tryReconnectWiFi(NetworkConfig config) {
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        if (millis() - lastWifiAttempt > retryInterval) {
            Serial.println("Tentative de reconnexion WiFi...");
            WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
            lastWifiAttempt = millis();
        }
    } else {
        wifiConnected = true;
    }
}

    

    void handle() {
       tryReconnectWiFi(configManager.getConfig());

        MQTTDevice::handle();
        checkSoundSensor();
                updateLED();  // Ajoutez cette ligne

        
    }
};

//ConfigManager configManager;
//NetworkConfig config;
MySmartHomeDevice device;


void setup() {
    Serial.begin(115200);
    Serial.println("App Launching");
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);


    if (!configManager.begin()) {
        Serial.println("Mode configuration AP actif");
        Serial.println("Connectez-vous au WiFi 'SmartHome-Config'");
        Serial.println("Ouvrez http://192.168.4.1 dans votre navigateur");

    }
  

    if (WiFi.hostByName("raspberrypi.local", MQTTBrokerip)) {
        Serial.print("IP du broker MQTT: ");
        Serial.println(MQTTBrokerip);
    } else {
        Serial.println("Échec de résolution DNS");
        delay(5000);
        }
    
    bool mqttConnected = device.begin(MQTTBrokerip);


    if (mqttConnected) {
        Serial.println("Connecté au broker MQTT!");
        device.setupHA();
        device.initPublish();


    } else {
        Serial.println("MQTT non disponible, tentative plus tard...");
       
    }
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

#endif