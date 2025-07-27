#ifdef MODULE_SENTINEL

#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include "MQTTDevice.h"
#include "ConfigManager.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#define DEBUG  1


ConfigManager configManager;

// Définition des broches
#define BUZZER_PIN 25
#define GAS_PIN 35
#define PRESENCE_PIN 32
#define LCD_SDA 21
#define LCD_SCL 22
#define DHT_PIN 33  
#define DHT_TYPE DHT11
IPAddress MQTTBrokerip;

#define LED_PIN 4  // Broche de la LED (D4/GPIO2 sur ESP8266)

// Variables pour le clignotement
unsigned long BlinkpreviousMillis = 0;
const long fastBlinkInterval = 200;  // Intervalle rapide (non connecté WiFi)
const long normalBlinkInterval = 500; // Intervalle normal (WiFi OK mais pas MQTT)
bool ledState = LOW;

DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastWifiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
const unsigned long retryInterval = 15000;

bool wifiConnected = false;
bool mqttConnected = false;
bool AlarmDeclenchedByHA = false;

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
        // Capteur d'humidité
        if(!getHAConfig().sendSensorConfig("cuisine", "humidite", "humidity", "%", "Humidité Cuisine")) {
            Serial.println("Échec de la configuration du capteur d'humidité");
        }                 
        
        // Capteur de gaz
        if(!getHAConfig().sendSensorConfig("cuisine", "gaz", "%", "Gaz Cuisine")) {
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
            AlarmDeclenchedByHA = (value == "ON");
            Serial.println("Commande reçue pour le buzzer: " + value);
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
    lcd.print("Gaz: ");
    lcd.print(readGasLevelPercentage());
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
    int readGasLevelPercentage() {
        int raw_level = analogRead(GAS_PIN);
        return map(constrain(raw_level,500,4095), 500, 4095, 0, 100);
    }

    bool readPresence() {
        return digitalRead(PRESENCE_PIN);
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
        }else if (!mqttConnection) {
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




    void handle() {
        MQTTDevice::handle();
       // tryReconnectWiFi(configManager.getConfig());
        updateLED();  // Ajoutez cette ligne

        
    }

};

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
KitchenDevice device;

void setup() {
    Serial.begin(115200);
    dht.begin();
    // Initialisation des broches
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GAS_PIN, INPUT);
    pinMode(PRESENCE_PIN, INPUT);
    pinMode(DHT_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
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



    if (millis() - lastUpdate > 1000) { // Toutes les 2 secondes
        // Lecture des capteurs
        float temperature = device.readTemperature();
        float humidity = dht.readHumidity();
        int gasLevel = device.readGasLevel();
        bool presence = device.readPresence();
        Serial.println("Température: "+String(temperature));
        Serial.println("Gaz: "+String(gasLevel));
        Serial.println("Présence: "+String(presence ? "ON" : "OFF"));
        
        int gasPercentage = device.readGasLevelPercentage();

        // Envoi des données
        device.publishSensorData("cuisine", "temperature", temperature);
        device.publishSensorData("cuisine", "humidite", humidity);
        device.publishSensorData("cuisine", "gaz", gasPercentage);
        device.publishSensorData("cuisine", "presence", presence ? String("ON") : String("OFF"));

        //  Détection d'incendie
    if (gasPercentage > 30 && temperature > 40) {
        // Tonalité rapide pour l'incendie
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println(" Incendie détecté !");
        device.publishSensorData("cuisine", "buzzer", String("ON"));
        }         // Gestion alarme gaz
        else  if (gasPercentage > 30) { // Seuil à ajuster
            digitalWrite(BUZZER_PIN, HIGH);
            Serial.println("Alerte gaz détecté !");
            device.publishSensorData("cuisine", "buzzer", String("ON"));

        } else {
    
            if(!AlarmDeclenchedByHA){digitalWrite(BUZZER_PIN, LOW);
                Serial.println("Gaz normal, alarme désactivée.");
                device.publishSensorData("cuisine", "buzzer", String("OFF"));}
            else {
                Serial.println("Gaz normal mais alarme maintenue par Home Assistant.");
            }

        }

        // Mise à jour LCD
        device.updateLCD();
        
        lastUpdate = millis();
    }
}
#endif