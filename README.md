# ESP32 Non-Blocking WiFiManager + MQTT + LittleFS + Custom IP

## Prerequisites

- **Hardware:** ESP32 board
- **Libraries:**
    - [WiFiManager](https://github.com/tzapu/WiFiManager)
    - [LittleFS_esp32](https://github.com/lorol/LITTLEFS)
    - [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
    - [PubSubClient](https://github.com/knolleary/pubsubclient)
    - [Button2](https://github.com/LennartHennigs/Button2)
    - [ezLED](https://github.com/raphaelbs/ezLED)
    - [TaskScheduler](https://github.com/arkhipenko/TaskScheduler)
- **Platform:** Arduino IDE or PlatformIO

## Features

- Non-blocking WiFi configuration portal using WiFiManager
- MQTT client with configurable broker, port, user, and password
- Persistent configuration storage using LittleFS
- Optional static IP configuration
- Button-triggered WiFi and config reset
- Status LED feedback
- Task-based scheduling for WiFiManager and MQTT
- Debug output for troubleshooting
