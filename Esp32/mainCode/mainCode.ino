#include <WiFi.h>
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
ConfigManager configManager;

// Définition des broches
#define DHT_PIN 4         // Broche digitale pour DHT11
#define WATER_LEVEL_PIN 34 // Broche analogique pour le capteur de niveau d'eau
#define SOIL_MOISTURE_PIN 35 // Broche analogique pour l'humidité du sol
#define PIR_PIN 2        // Broche digitale pour le capteur de présence
#define MQ2_PIN 32        // Broche analogique pour MQ2
#define Buzzer_PIN 5
#define RGB_R_PIN 25
#define RGB_G_PIN 26
#define RGB_B_PIN 27

#include <DHT.h>
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ==================== CLASSE POUR LA SIGNALISATION ====================
class DeviceIndicator {
private:
    // États de l'appareil
    enum DeviceState {
        STARTUP,
        Device_WIFI_CONNECTING,
        WIFI_CONNECTED,
        MQTT_CONNECTING,
        Device_MQTT_CONNECTED,
        NORMAL_OPERATION,
        ALERT_GAS,
        ALERT_WATER_LOW,
        ALERT_SOIL_DRY,
        ERROR_CONNECTION,
        CONFIG_MODE
    };
    
    DeviceState currentState;
    unsigned long lastBlink;
    bool blinkState;
    unsigned long alertStartTime;
    bool alertActive;
    
public:
    DeviceIndicator() : currentState(STARTUP), lastBlink(0), blinkState(false), alertStartTime(0), alertActive(false) {}
    
    void begin() {
        // Configuration des broches
        pinMode(RGB_R_PIN, OUTPUT);
        pinMode(RGB_G_PIN, OUTPUT);
        pinMode(RGB_B_PIN, OUTPUT);
        pinMode(Buzzer_PIN, OUTPUT);
        
        // Test initial
        testIndicators();
        setStartup();
    }
    
    void testIndicators() {
        Serial.println("Test des indicateurs...");
        
        // Test RGB
        setRGB(255, 0, 0); delay(300);    // Rouge
        setRGB(0, 255, 0); delay(300);    // Vert
        setRGB(0, 0, 255); delay(300);    // Bleu
        setRGB(0, 0, 0);                  // Éteint
        
        // Test buzzer
        tone(Buzzer_PIN, 1000, 200);
        delay(300);
        tone(Buzzer_PIN, 1500, 200);
        delay(300);
        
        Serial.println("Test terminé.");
    }
    
    void setRGB(int r, int g, int b) {
        analogWrite(RGB_R_PIN, r);
        analogWrite(RGB_G_PIN, g);
        analogWrite(RGB_B_PIN, b);
    }
    
    void playTone(int frequency, int duration) {
        tone(Buzzer_PIN, frequency, duration);
    }
    
    void playMelody(int notes[], int durations[], int length) {
        for (int i = 0; i < length; i++) {
            tone(Buzzer_PIN, notes[i], durations[i]);
            delay(durations[i] * 1.3);
        }
    }
    
    // États de l'appareil
    void setStartup() {
        currentState = STARTUP;
        setRGB(255, 0, 255); // Magenta
        playTone(800, 100);
        Serial.println("État: STARTUP");
    }
    
    void setWifiConnecting() {
        currentState = Device_WIFI_CONNECTING;
        Serial.println("État: WIFI_CONNECTING");
    }
    
    void setWifiConnected() {
        currentState = WIFI_CONNECTED;
        setRGB(0, 0, 255); // Bleu
        playTone(1000, 200);
        Serial.println("État: WIFI_CONNECTED");
    }
    
    void setMqttConnecting() {
        currentState = MQTT_CONNECTING;
        Serial.println("État: MQTT_CONNECTING");
    }
    
    void setMqttConnected() {
        currentState = Device_MQTT_CONNECTED;
        setRGB(0, 255, 0); // Vert
        int notes[] = {1000, 1200, 1400};
        int durations[] = {100, 100, 200};
        playMelody(notes, durations, 3);
        Serial.println("État: MQTT_CONNECTED");
    }
    
    void setNormalOperation() {
        currentState = NORMAL_OPERATION;
        setRGB(0, 50, 0); // Vert faible (respiration)
        Serial.println("État: NORMAL_OPERATION");
    }
    
    void setConfigMode() {
        currentState = CONFIG_MODE;
        Serial.println("État: CONFIG_MODE");
    }
    
    void setConnectionError() {
        currentState = ERROR_CONNECTION;
        setRGB(255, 0, 0); // Rouge
        playTone(400, 500);
        Serial.println("État: ERROR_CONNECTION");
    }
    
    // Alertes
    void setGasAlert() {
        currentState = ALERT_GAS;
        alertActive = true;
        alertStartTime = millis();
        Serial.println("ALERTE: Gaz détecté!");
    }
    
    void setWaterLowAlert() {
        currentState = ALERT_WATER_LOW;
        alertActive = true;
        alertStartTime = millis();
        Serial.println("ALERTE: Niveau d'eau bas!");
    }
    
    void setSoilDryAlert() {
        currentState = ALERT_SOIL_DRY;
        alertActive = true;
        alertStartTime = millis();
        Serial.println("ALERTE: Sol sec!");
    }
    
    void clearAlerts() {
        if (alertActive) {
            alertActive = false;
            setNormalOperation();
            Serial.println("Alertes effacées - retour à l'état normal");
        }
    }
    
    // Fonction principale à appeler dans loop()
    void update() {
        unsigned long currentTime = millis();
        
        // Gestion des clignotements
        if (currentTime - lastBlink > 500) {
            blinkState = !blinkState;
            lastBlink = currentTime;
        }
        
        switch (currentState) {
            case Device_WIFI_CONNECTING:
                // Clignotement bleu
                if (blinkState) {
                    setRGB(0, 0, 255);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case MQTT_CONNECTING:
                // Clignotement cyan
                if (blinkState) {
                    setRGB(0, 255, 255);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case NORMAL_OPERATION:
                // Respiration verte douce
                {
                    int brightness = (sin(currentTime / 1000.0) + 1) * 25;
                    setRGB(0, brightness, 0);
                }
                break;
                
            case CONFIG_MODE:
                // Clignotement orange
                if (blinkState) {
                    setRGB(255, 165, 0);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case ERROR_CONNECTION:
                // Clignotement rouge rapide
                if (currentTime - lastBlink > 250) {
                    blinkState = !blinkState;
                    lastBlink = currentTime;
                }
                if (blinkState) {
                    setRGB(255, 0, 0);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case ALERT_GAS:
                // Alerte gaz - rouge clignotant + buzzer
                if (blinkState) {
                    setRGB(255, 0, 0);
                    playTone(2000, 100);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case ALERT_WATER_LOW:
                // Alerte eau - bleu clignotant + buzzer
                if (blinkState) {
                    setRGB(0, 0, 255);
                    playTone(1000, 200);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
                
            case ALERT_SOIL_DRY:
                // Alerte sol sec - jaune clignotant + buzzer
                if (blinkState) {
                    setRGB(255, 255, 0);
                    playTone(1500, 150);
                } else {
                    setRGB(0, 0, 0);
                }
                break;
        }
        
        // Auto-clear des alertes après 30 secondes
        if (alertActive && (currentTime - alertStartTime > 30000)) {
            clearAlerts();
        }
    }
    
    // Notification de présence détectée
    void notifyPresence() {
        if (currentState == NORMAL_OPERATION) {
            // Flash blanc rapide
            setRGB(255, 255, 255);
            playTone(1200, 50);
            delay(100);
            setRGB(0, 50, 0); // Retour à l'état normal
        }
    }
    
    // Notification de données envoyées
    void notifyDataSent() {
        if (currentState == NORMAL_OPERATION) {
            // Flash vert très rapide
            setRGB(0, 255, 0);
            delay(50);
            setRGB(0, 50, 0);
        }
    }
};

// Instance globale de l'indicateur
DeviceIndicator indicator;

class MySmartHomeDevice : public MQTTDevice {
public:
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
        // Capteur de température
        if(!getHAConfig().sendSensorConfig("salon", "temperature", "temperature", "°C", "Température Salon")) {
            Serial.println("Échec de la configuration du capteur de température");
        } else {
            Serial.println("Succès de la configuration du capteur de température");
        }
        
        delay(500);
        
        // Capteur d'humidité
        if(!getHAConfig().sendSensorConfig("salon", "humidite", "humidity", "%", "Humidité Salon")) {
            Serial.println("Échec de la configuration du capteur d'humidité");
        } else {
            Serial.println("Succès de la configuration du capteur d'humidité");
        }
        
        delay(500);
        
        // Capteur de gaz MQ2
        if (!getHAConfig().sendSensorConfig("salon", "gaz","ppm", "Détection de gaz (MQ2)")) {
            Serial.println("Échec de la configuration du capteur de gaz");
        } else {
            Serial.println("Succès de la configuration du capteur de gaz");
        }
        delay(500);

        // Capteur d'humidité du sol
        if (!getHAConfig().sendSensorConfig("salon", "humidite_sol", "moisture", "%", "Humidité du sol")) {
            Serial.println("Échec de la configuration du capteur d'humidité du sol");
        } else {
            Serial.println("Succès de la configuration du capteur d'humidité du sol");
        }
        delay(500);

        // Capteur de niveau d'eau
        if (!getHAConfig().sendSensorConfig("salon", "niveau_eau", "moisture", "%", "Niveau d'eau")) {
            Serial.println("Échec de la configuration du capteur de niveau d'eau");
        } else {
            Serial.println("Succès de la configuration du capteur de niveau d'eau");
        }
        delay(500);

        // Capteur PIR
        if (!getHAConfig().sendBinarySensorConfig("salon", "presence", "motion", "Présence détectée")) {
            Serial.println("Échec de la configuration du capteur PIR");
        } else {
            Serial.println("Succès de la configuration du capteur PIR");
        }
    }

    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "lampe") {
            Serial.print("Commande lampe reçue: ");
            Serial.println(value);
            publishSensorData(location, device, value);
            Serial.printf("Commande Lampe reçue et exécutée %s dans la %s\n", value.c_str(), location.c_str());
        }
    }
};
// === Simulation Température & Humidité ===
float simulatedTemperature = 28.0;
float simulatedHumidity = 60.0;
unsigned long lastSimUpdate = 0;
const unsigned long simInterval = 5000; // mise à jour toutes les 5 secondes
bool warmingUp = true;
bool coolingDown = false;


void simulateSensorData(float &temperature, float &humidity) {
    unsigned long now = millis();
    if (now - lastSimUpdate >= simInterval) {
        lastSimUpdate = now;

        // Simulation température stable mais évolutive
        if (warmingUp) {
            simulatedTemperature += 0.2;
            if (simulatedTemperature >= 31.5) {
                warmingUp = false;
                coolingDown = true;
            }
        } else if (coolingDown) {
            simulatedTemperature -= 0.1;
            if (simulatedTemperature <= 28.0) {
                coolingDown = false;
                warmingUp = true;
            }
        }

        // Humidité avec petite fluctuation
        simulatedHumidity += (random(-2, 3) * 0.1);
        if (simulatedHumidity < 50) simulatedHumidity = 50;
        if (simulatedHumidity > 70) simulatedHumidity = 70;
    }

    // Retourne les valeurs simulées
    temperature = simulatedTemperature;
    humidity = simulatedHumidity;
}

MySmartHomeDevice device;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("App Launching");
    
    // Initialiser les indicateurs
    indicator.begin();
    
    // Mode configuration AP si nécessaire
    if (!configManager.begin()) {
        Serial.println("Mode configuration AP actif");
        Serial.println("Connectez-vous au WiFi 'SmartHome-Config'");
        Serial.println("Ouvrez http://192.168.4.1 dans votre navigateur");
        indicator.setConfigMode();
        
        while (!configManager.isConfigured()) {
            configManager.handleClient();
            indicator.update();
            delay(100);
        }
    }

    // Récupération de la configuration
    NetworkConfig config = configManager.getConfig();
    
    Serial.print("Tentative de connexion à: ");
    Serial.println(config.wifiSSID);
    
    // Connexion WiFi avec indication visuelle
    indicator.setWifiConnecting();
    WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
    
    unsigned long wifiTimeout = millis() + 30000;
    while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
        indicator.update();
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nErreur de connexion WiFi!");
        indicator.setConnectionError();
        delay(5000);
        ESP.restart();
    }

    Serial.println("\nConnecté au WiFi!");
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());
    indicator.setWifiConnected();
    delay(1000);

    // Configuration MQTT
    IPAddress MQTTBrokerip;
    indicator.setMqttConnecting();
    
    if (WiFi.hostByName("raspberrypi.local", MQTTBrokerip)) {
        Serial.print("IP du broker MQTT: ");
        Serial.println(MQTTBrokerip);
    } else {
        Serial.println("Échec de résolution DNS");
        indicator.setConnectionError();
        delay(5000);
        ESP.restart();
    }
    
    bool mqttConnected = device.begin(MQTTBrokerip);
    
    if (mqttConnected) {
        Serial.println("Connecté au broker MQTT!");
        indicator.setMqttConnected();
        delay(1000);
    } else {
        Serial.println("Échec de la connexion au broker MQTT!");
        indicator.setConnectionError();
        delay(5000);
        ESP.restart();
    }

    // Configuration des périphériques
    device.setupHA();
    dht.begin();
    pinMode(PIR_PIN, INPUT);
    
    // État normal
    indicator.setNormalOperation();
    Serial.println("Setup complet!");
}

void loop() {
    // Mettre à jour les indicateurs
    indicator.update();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connexion WiFi perdue, tentative de reconnexion...");
        indicator.setWifiConnecting();
        WiFi.reconnect();
        delay(5000);
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Échec reconnexion, redémarrage...");
            indicator.setConnectionError();
            delay(2000);
            ESP.restart();
        } else {
            indicator.setWifiConnected();
            delay(1000);
            indicator.setNormalOperation();
        }
    }
    
    configManager.handleClient();
    
    if (configManager.isConfigured()) {
        device.handle();
        
        // === Lecture des capteurs ===
        // float humidity = dht.readHumidity();
        // float temperature = dht.readTemperature();
        // delay(200);
         float humidity = 0;
         float temperature = 0;
            simulateSensorData(temperature, humidity);  // <== APPEL DE LA SIMULATION
            delay(200);

        
        int waterLevel = analogRead(WATER_LEVEL_PIN);
        int waterLevelPercentage = map(waterLevel, 0, 4095, 0, 100);
        delay(200);

        int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
        int soilMoisturePercentage = map(soilMoisture, 4095, 0, 0, 100);
        delay(200);

        int presence = digitalRead(PIR_PIN);
        delay(200);

        int gasValue = analogRead(MQ2_PIN);
        int gasPercentage = map(gasValue, 0, 4095, 0, 100);
        delay(200);

        // === Gestion des alertes ===
        static bool lastAlertState = false;
        bool currentAlertState = false;
        
        // Vérification des seuils d'alerte
        if (gasPercentage > 30) {
            indicator.setGasAlert();
            currentAlertState = true;
        } else if (waterLevelPercentage < 20) {
            indicator.setWaterLowAlert();
            currentAlertState = true;
        } else if (soilMoisturePercentage < 20) {
           // indicator.setSoilDryAlert();
           // currentAlertState = true;
        }
        
        // Effacer les alertes si les conditions sont revenues normales
        if (lastAlertState && !currentAlertState) {
            indicator.clearAlerts();
        }
        lastAlertState = currentAlertState;
        
        // === Notification de présence ===
        static int lastPresence = 0;
        if (presence && !lastPresence) {
            indicator.notifyPresence();
        }
        lastPresence = presence;

        // === Envoi des données ===
        static unsigned long lastSend = 0;
        if (millis() - lastSend > 1000) {
            device.publishSensorData("salon", "temperature", temperature);
            device.publishSensorData("salon", "humidite", humidity);
            device.publishSensorData("salon", "niveau_eau", waterLevelPercentage);
            device.publishSensorData("salon", "humidite_sol", soilMoisturePercentage);
            device.publishSensorData("salon", "gaz", gasPercentage);
            device.publishSensorData("salon", "presence", presence ?"ON":"OFF");
            
            // Notification visuelle d'envoi de données
            indicator.notifyDataSent();
            
            lastSend = millis();
        }
    }
}