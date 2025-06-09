ESP32 Non-Blocking WiFiManager + MQTT + LittleFS + Custom IP

## Prerequisites

- ESP32 board (tested with ESP32 Dev Module)
- Arduino IDE or PlatformIO
- Install the following libraries:
    - [WiFiManager by tzapu](https://github.com/tzapu/WiFiManager)
    - LittleFS
    - [ArduinoJson](https://arduinojson.org/)
    - [PubSubClient](https://github.com/knolleary/pubsubclient)
    - [Button2](https://github.com/LennartHennigs/Button2)
    - [ezLED](https://github.com/raphaelbs/ezLED)
    - [TickTwo](https://github.com/RobTillaart/TickTwo) 
    

## Features

- Non-blocking WiFiManager configuration portal for WiFi and MQTT credentials
- MQTT client with dynamic broker/user/pass/port configuration
- LittleFS for persistent storage of configuration (JSON)
- Custom static IP, gateway, subnet, and DNS support
- Reset button (long press) to clear WiFi and MQTT settings
- Status LED feedback (using ezLED)
- MQTT subscribe/publish example hooks
- Debug output via serial (toggle with `_DEBUG_` macro)
- Modular, event-driven architecture using TickTwo for non-blocking tasks
