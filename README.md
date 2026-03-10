# RonoBox — Local Edge IoT Home Automation

> A modular IoT home automation system built on ESP32/ESP8266 that works **entirely offline** — no cloud required.

RonoBox is the embedded firmware powering a smart home architecture designed for environments with unstable or limited internet connectivity — and as a general-purpose alternative to cloud-dependent systems. It runs locally on a Raspberry Pi broker and integrates natively with **Home Assistant** via **MQTT**.

The goal of this project is to make home automation **accessible, reliable, and deployable** in regions where internet connectivity is inconsistent — starting with Africa.

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Hardware Requirements](#hardware-requirements)
- [Available Modules](#available-modules)
- [Library Architecture](#library-architecture)
- [Setup & Installation](#setup--installation)
- [First Boot — Captive Portal Configuration](#first-boot--captive-portal-configuration)
- [MQTT Topic Structure](#mqtt-topic-structure)
- [Home Assistant Integration](#home-assistant-integration)
- [Related Repository](#related-repository)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                 Local WiFi Network                       │
│                                                          │
│  ESP32 / ESP8266           Raspberry Pi                  │
│  ┌─────────────┐           ┌──────────────────────┐      │
│  │  RonoBox    │──MQTT────▶│  Mosquitto Broker    │      │
│  │  Firmware   │           │  Home Assistant      │      │
│  │             │◀─MQTT─────│  InfluxDB + Grafana  │      │
│  └─────────────┘           └──────────────────────┘      │
│                                                          │
│  No internet required for core functionality             │
└─────────────────────────────────────────────────────────┘
```

Each ESP device:
1. **Auto-configures** via a captive portal on first boot
2. **Connects** to the local Raspberry Pi MQTT broker (resolved via `raspberrypi.local`)
3. **Registers** its sensors/switches in Home Assistant automatically (MQTT Discovery)
4. **Publishes** sensor data and **receives** commands in real time

---

## Hardware Requirements

| Component | Details |
|---|---|
| Microcontroller | ESP32 (NodeMCU-32S) or ESP8266 (NodeMCU v2) |
| MQTT Broker | Raspberry Pi running Mosquitto |
| Home Assistant | Running on the same Raspberry Pi |

**Module-specific hardware:**

| Module | Additional Hardware |
|---|---|
| Smart Plug | Relay module, LED indicator |
| Night Light | Relay module, sound sensor, DHT11/DHT22, LED |
| Kitchen Sentinel | Gas sensor (MQ-2/analog), PIR presence sensor, DHT11, I2C LCD 16×2, buzzer |

---

## Available Modules

Modules are selected at **compile time** via build flags in `platformio.ini`. Only one module is active per device.

### 1. Smart Plug (`MODULE_SMARTPLUG`) — ESP8266

Controls a relay (normally closed configuration) remotely via Home Assistant.

- Relay control (ON/OFF)
- LED status indicator (WiFi / MQTT connection states)
- Home Assistant switch integration

### 2. Night Light (`MODULE_VEILLEUSE`) — ESP8266

Automatically controls a lamp based on ambient sound and publishes environmental data.

- Sound-triggered relay control
- Temperature & humidity publishing (DHT sensor)
- LED status indicator

### 3. Kitchen Sentinel (`MODULE_SENTINEL`) — ESP32

Multi-sensor safety module for kitchen monitoring.

- **Gas detection** (analog, mapped to 0–100%)
- **Fire detection** (combined gas % + temperature threshold)
- **Presence detection** (PIR sensor)
- **Temperature & humidity** (DHT11)
- **16×2 I2C LCD** — live readings display
- **Buzzer alarm** — auto-triggered or controlled via Home Assistant
- Full Home Assistant integration (sensors + binary sensors + switch)

### 4. Latency Test (`LATENCY_TEST_AWS` / `LATENCY_TEST`) — ESP32

Diagnostic module for measuring network round-trip latency, including AWS endpoint testing.

---

## Library Architecture

The firmware is built around 4 custom libraries in `/lib`:

```
lib/
├── ConfigManager/       WiFi + MQTT configuration with captive portal
├── MQTTDevice/          Abstract base class for all device modules
├── MQTTTopicManager/    Standardized MQTT topic generation & publishing
└── HADiscoveryConfig/   Home Assistant MQTT Discovery auto-registration
```

### `ConfigManager`

Handles WiFi and MQTT credentials storage and the first-boot captive portal.

- **ESP32**: stores config in `Preferences` (NVS flash)
- **ESP8266**: stores config in EEPROM
- On first boot (no config found): launches `SmartHome-Config` WiFi access point
- Web portal at `192.168.4.1` — enter WiFi SSID/password and MQTT broker details

### `MQTTDevice` (abstract base class)

All device modules inherit from this class. It handles:

- WiFi client + PubSubClient initialization
- Automatic MQTT reconnection (every 10 seconds)
- Topic subscription management
- Unified `publishSensorData()` methods (float, int, String, bool)
- Abstract `handleCommand()` — implemented by each module

### `MQTTTopicManager`

Generates consistent MQTT topics based on device MAC address and location:

```
home/{location}/{platform}-{mac}/{sensor}/{type}
```

Example: `home/cuisine/esp32-AABBCCDDEEFF/temperature/state`

### `HADiscoveryConfig`

Sends MQTT Discovery configuration payloads to Home Assistant so sensors, switches, and binary sensors appear automatically — no manual HA configuration needed.

Supported entity types:
- `sensor` (with optional device class and unit)
- `binary_sensor` (motion, etc.)
- `switch` (relay, buzzer)

---

## Setup & Installation

### Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
- This project uses **PlatformIO**, not the Arduino IDE

> ⚠️ Do **not** use the Arduino IDE. PlatformIO manages dependencies, multi-environment builds, and OTA in a way the Arduino IDE does not support.

### Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/RonaldPrecieux/RonoBox.git
   cd RonoBox
   ```

2. **Open in VS Code** — PlatformIO will detect `platformio.ini` and install dependencies automatically

3. **Select your module** — edit `platformio.ini` to enable the desired module:

   ```ini
   ; For ESP8266 — Smart Plug
   [env:esp8266]
   build_flags =
       -DESP8266
       -DMODULE_SMARTPLUG

   ; For ESP32 — Kitchen Sentinel
   [env:esp32]
   build_flags =
       -DESP32
       -DMODULE_SENTINEL
   ```

   Uncomment the module flag you want, comment out the others.

4. **Build and upload** — use PlatformIO's Upload button or:
   ```bash
   pio run -e esp8266 --target upload    # For ESP8266
   pio run -e esp32 --target upload      # For ESP32
   ```

5. **First boot** — follow the [captive portal configuration](#first-boot--captive-portal-configuration) below

---

## First Boot — Captive Portal Configuration

On first boot (or after a config reset), the device launches a WiFi access point:

| Setting | Value |
|---|---|
| SSID | `SmartHome-Config` |
| Password | `configureme` |
| Portal URL | `http://192.168.4.1` |

Connect to `SmartHome-Config` from your phone or laptop, then open `192.168.4.1` in your browser. Fill in:

- **WiFi SSID** and **password** of your home network
- **MQTT Broker** — use `raspberrypi.local` (resolved automatically via mDNS) or the Raspberry Pi's local IP

Once saved, the device restarts and connects automatically. Configuration is persisted in flash memory.

To **reset** the configuration, call `configManager.resetConfiguration()` in your code or hold a designated reset button (implementation specific to each module).

---

## MQTT Topic Structure

```
home/{location}/{platform}-{mac}/{sensor}/{type}
```

| Segment | Example values |
|---|---|
| `location` | `salon`, `cuisine`, `testlatence` |
| `platform-mac` | `esp32-AABBCCDDEEFF`, `esp8266-AABBCCDD` |
| `sensor` | `temperature`, `humidite`, `gaz`, `presence`, `buzzer` |
| `type` | `state` (publish) or `set` (receive commands) |

**Examples:**

```
home/cuisine/esp32-AABBCCDDEEFF/temperature/state   → "23.5"
home/cuisine/esp32-AABBCCDDEEFF/presence/state      → "ON"
home/cuisine/esp32-AABBCCDDEEFF/buzzer/set          → "ON"   (command from HA)
```

State messages are **retained** — Home Assistant always shows the last known value after restart.

---

## Home Assistant Integration

RonoBox uses **MQTT Discovery** — entities appear in Home Assistant automatically with no YAML configuration.

Discovery topics follow the standard HA format:
```
homeassistant/{entity_type}/{entity_name}/config
```

On startup, each module calls `setupHA()` which sends configuration payloads for all its sensors. Home Assistant picks them up within seconds.

**Kitchen Sentinel entities in HA:**
- `sensor.temperature_cuisine` (°C)
- `sensor.humidite_cuisine` (%)
- `sensor.gaz_cuisine` (%)
- `binary_sensor.presence_cuisine` (motion)
- `switch.alarme_cuisine` (buzzer on/off)

---

## Creating Your Own Module

The library is designed to be extended. Here is a minimal example showing how to build a new device module using `MQTTDevice` as a base class.

### 1. Inherit from `MQTTDevice`

```cpp
#include "MQTTDevice.h"
#include "ConfigManager.h"

ConfigManager configManager;

class MyDevice : public MQTTDevice {
public:
    MyDevice() : MQTTDevice(getMacAddress()) {}

    // Required: handle incoming MQTT commands
    void handleCommand(const String& location, const String& device, const String& value) override {
        if (device == "relay") {
            digitalWrite(RELAY_PIN, value == "ON" ? HIGH : LOW);
            publishSensorData(location, device, value);
        }
    }

    // Register your entities in Home Assistant
    void setupHA() {
        getHAConfig().sendSwitchConfig("salon", "relay", "Mon Relais");
        getHAConfig().sendSensorConfig("salon", "temperature", "temperature", "°C", "Température Salon");
    }

private:
    String getMacAddress() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18] = {0};
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }
};

MyDevice device;

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);

    // First boot: launches "SmartHome-Config" WiFi AP for configuration
    configManager.begin();

    // Connect to MQTT broker (resolved via mDNS)
    IPAddress brokerIp;
    WiFi.hostByName("raspberrypi.local", brokerIp);
    if (device.begin(brokerIp)) {
        device.setupHA();  // Auto-register in Home Assistant
    }
}

void loop() {
    configManager.handleClient();  // Serve captive portal if active
    device.handle();               // MQTT loop + auto-reconnect

    // Publish sensor data every 5 seconds
    static unsigned long last = 0;
    if (millis() - last > 5000) {
        device.publishSensorData("salon", "temperature", readTemperature());
        last = millis();
    }
}
```

### 2. Enable your module in `platformio.ini`

```ini
[env:esp32]
platform = espressif32
board = nodemcu-32s
framework = arduino
build_flags =
    -DESP32
    -DMODULE_MY_DEVICE
lib_deps =
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^7.4.2
```

### 3. Wrap your module with the build flag

```cpp
#ifdef MODULE_MY_DEVICE
// ... your full module code here
#endif
```

This ensures only one module is compiled per device — keeping binary size minimal on constrained hardware.

---

## Contributing

Contributions are welcome, especially for:

- **New device modules** (window sensors, door locks, smart meters...)
- **OTA update support** — push firmware updates over WiFi without USB
- **Multi-broker support** — connect to both local and remote brokers simultaneously
- **Encryption** — TLS/SSL MQTT connections for deployments exposed to the internet
- **Battery-powered device support** — deep sleep strategies for ESP8266 on battery

Open a PR or an issue — the goal is to make local smart home infrastructure accessible everywhere.

---

## Related Repository

The Raspberry Pi server stack (Mosquitto MQTT broker, Home Assistant, InfluxDB, Grafana) is configured separately:

→ **[Rasp-Pi](https://github.com/RonaldPrecieux/Rasp-Pi)** — Raspberry Pi server configuration & setup

---

## License

MIT
