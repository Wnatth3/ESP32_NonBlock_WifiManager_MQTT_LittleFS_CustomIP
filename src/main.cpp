
/*
- ***LittleFS library conflicts with DebugMode. Result to ezLED and Button2 Libraries are not working.
*/

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <TickTwo.h>
#include <ezLED.h>
#include <Button2.h>

//******************************** Configulation ****************************//
#define FORMAT_LITTLEFS_IF_FAILED true

#define _DEBUG_

//******************************** Variables & Objects **********************//
#define deviceName "MyESP32"

const char* filename = "/config.txt";  // Config file name

bool storedValues;

//----------------- esLED ---------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi Manager --------------//
char mqttBroker[16] = "192.168.0.10";
char mqttPort[6]    = "1883";
char mqttUser[10];
char mqttPass[10];

WiFiManager wifiManager;

WiFiManagerParameter customMqttBroker("broker", "mqtt server", mqttBroker, 16);
WiFiManagerParameter customMqttPort("port", "mqtt port", mqttPort, 6);
WiFiManagerParameter customMqttUser("user", "mqtt user", mqttUser, 10);
WiFiManagerParameter customMqttPass("pass", "mqtt pass", mqttPass, 10);

//----------------- MQTT ----------------------//
WiFiClient   espClient;
PubSubClient mqtt(espClient);

//******************************** Tasks ************************************//
void    connectMqtt();
void    reconnectMqtt();
TickTwo tConnectMqtt(connectMqtt, 0, 0, MILLIS);  // (function, interval, iteration, interval unit)
TickTwo tReconnectMqtt(reconnectMqtt, 3000, 0, MILLIS);

//******************************** Functions ********************************//
//----------------- LittleFS ------------------//
// Loads the configuration from a file
void loadConfiguration(fs::FS& fs, const char* filename) {
    // Open file for reading
    File file = fs.open(filename, "r");
    if (!file) {
#ifdef _DEBUG_
        Serial.println(F("Failed to open data file"));
#endif
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
#ifdef _DEBUG_
        Serial.println(F("Failed to read file, using default configuration"));
#endif
    }
    // Copy values from the JsonDocument to the Config
    // strlcpy(Destination_Variable, doc["Source_Variable"] /*| "Default_Value"*/, sizeof(Destination_Name));
    strlcpy(mqttBroker, doc["mqttBroker"], sizeof(mqttBroker));
    strlcpy(mqttPort, doc["mqttPort"], sizeof(mqttPort));
    strlcpy(mqttUser, doc["mqttUser"], sizeof(mqttUser));
    strlcpy(mqttPass, doc["mqttPass"], sizeof(mqttPass));
    storedValues = doc["storedValues"];

    file.close();
}

// Saves the configuration to a file
void saveConfiguration(fs::FS& fs, const char* filename) {
    strcpy(mqttBroker, customMqttBroker.getValue());
    strcpy(mqttPort, customMqttPort.getValue());
    strcpy(mqttUser, customMqttUser.getValue());
    strcpy(mqttPass, customMqttPass.getValue());
#ifdef _DEBUG_
    Serial.println(F("The values updated."));
#endif

    // Delete existing file, otherwise the configuration is appended to the file
    // fs.remove(filename);

    File file = fs.open(filename, "w");
    if (!file) {
#ifdef _DEBUG_
        Serial.println(F("Failed to open config file for writing"));
#endif
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Set the values in the document
    doc["mqttBroker"]   = mqttBroker;
    doc["mqttPort"]     = mqttPort;
    doc["mqttUser"]     = mqttUser;
    doc["mqttPass"]     = mqttPass;
    doc["storedValues"] = true;
#ifdef _DEBUG_
    Serial.print(F("The configuration has been saved to "));
    Serial.println(filename);
#endif

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
#ifdef _DEBUG_
        Serial.println(F("Failed to write to file"));
#endif
    }

    file.close();  // Close the file
#ifdef _DEBUG_
    Serial.println(F("Configuration saved"));
#endif

    mqtt.setServer(mqttBroker, atoi(mqttPort));
#ifdef _DEBUG_
    Serial.print(F("set MQTT Broker: "));
    Serial.println(mqttBroker);
#endif
    tConnectMqtt.start();
}

void saveParamsCallback() { saveConfiguration(LittleFS, filename); }

// Prints the content of a file to the Serial
void printFile(fs::FS& fs, const char* filename) {
    // Open file for reading
    File file = fs.open(filename, "r");
    if (!file) {
#ifdef _DEBUG_
        Serial.println(F("Failed to open data file"));
#endif
        return;
    }

    // Extract each characters by one by one
    while (file.available()) {
        Serial.print((char)file.read());
    }
    Serial.println();

    file.close();  // Close the file
}

void deleteFile(fs::FS& fs, const char* path) {
#ifdef _DEBUG_
    Serial.print(F("Deleting file: "));
    Serial.println(String(path) + "\r\n");
#endif
    if (fs.remove(path)) {
#ifdef _DEBUG_
        Serial.println(F("- file deleted"));
#endif
    } else {
#ifdef _DEBUG_
        Serial.println(F("- delete failed"));
#endif
    }
}

//----------------- Wifi Manager --------------//
void wifiManagerSetup() {
#ifdef _DEBUG_
    Serial.println(F("Loading configuration..."));
#endif
    loadConfiguration(LittleFS, filename);

    // add all your parameters here
    wifiManager.addParameter(&customMqttBroker);
    wifiManager.addParameter(&customMqttPort);
    wifiManager.addParameter(&customMqttUser);
    wifiManager.addParameter(&customMqttPass);

    wifiManager.setDarkMode(true);
    // wifiManager.setConfigPortalTimeout(60);
    wifiManager.setConfigPortalBlocking(false);
#ifdef _DEBUG_
    Serial.println(F("Saving configuration..."));
#endif
    wifiManager.setSaveParamsCallback(saveParamsCallback);
#ifdef _DEBUG_
    Serial.println(F("Print config file..."));
#endif
    printFile(LittleFS, filename);

    if (wifiManager.autoConnect(deviceName, "password")) {
#ifdef _DEBUG_
        Serial.println(F("connected...yeey :)"));
#endif
    } else {
#ifdef _DEBUG_
        Serial.println(F("Configportal running"));
#endif
    }
}

void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == "test/subscribe/topic") {
        if (message == "aValue") {
            // Do something
        } else if (message == "otherValue") {
            // Do something
        }
    }
}

void subscribeMqtt() {
#ifdef _DEBUG_
    Serial.println(F("Subscribing to the MQTT topics..."));
#endif
    mqtt.subscribe("test/subscribe/topic");
}

void publishMqtt() {
#ifdef _DEBUG_
    Serial.println(F("Publishing to the MQTT topics..."));
#endif
    mqtt.publish("test/publish/topic", "Hello World!");
}

//----------------- Connect MQTT --------------//
void reconnectMqtt() {
    if (WiFi.status() == WL_CONNECTED) {
#ifdef _DEBUG_
        Serial.print(F("Connecting MQTT... "));
#endif
        if (mqtt.connect(deviceName, mqttUser, mqttPass)) {
            tReconnectMqtt.stop();
#ifdef _DEBUG_
            Serial.println(F("connected"));
#endif
            tConnectMqtt.interval(0);
            tConnectMqtt.start();
            statusLed.blinkNumberOfTimes(200, 200, 3);  // 250ms ON, 750ms OFF, repeat 3 times, blink immediately
            subscribeMqtt();
            publishMqtt();
        } else {
#ifdef _DEBUG_
            Serial.print(F("failed state: "));
            Serial.println(mqtt.state());
            Serial.print(F("counter: "));
            Serial.println(tReconnectMqtt.counter());
#endif
            if (tReconnectMqtt.counter() >= 3) {
                // ESP.restart();
                tReconnectMqtt.stop();
                // tConnectMqtt.interval(3600 * 1000);  // Wait 1 hour before reconnecting.
                tConnectMqtt.interval(60 * 1000);  // Wait 1 minute before reconnecting.
                tConnectMqtt.resume();
            }
        }
    } else {
        if (tReconnectMqtt.counter() <= 1) {
#ifdef _DEBUG_
            Serial.println(F("WiFi is not connected"));
#endif
        }
    }
}

void connectMqtt() {
    if (!mqtt.connected()) {
        tConnectMqtt.pause();
        tReconnectMqtt.start();
    } else {
        mqtt.loop();
    }
}

//----------------- Reset WiFi Button ---------//
void resetWifiBtPressed(Button2& btn) {
    statusLed.turnON();
#ifdef _DEBUG_
    Serial.println(F("Deleting the config file and resetting WiFi."));
#endif
    deleteFile(LittleFS, filename);
    wifiManager.resetSettings();
    Serial.print(deviceName);
#ifdef _DEBUG_
    Serial.println(F(" is restarting."));
#endif
    ESP.restart();
}
//********************************  Setup ***********************************//
void setup() {
    statusLed.turnOFF();
    resetWifiBt.begin(resetWifiBtPin);
    resetWifiBt.setLongClickTime(5000);
    resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);
    Serial.begin(115200);
    while (!Serial)
        continue;
    // Initialize LittleFS library
    while (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
#ifdef _DEBUG_
        Serial.println(F("Failed to initialize LittleFS library"));
#endif
        delay(1000);
    }

    wifiManagerSetup();
    mqtt.setCallback(handleMqttMessage);

    if (storedValues) {
        mqtt.setServer(mqttBroker, atoi(mqttPort));
        tConnectMqtt.start();
    }
}

//********************************  Loop ************************************//
void loop() {
    statusLed.loop();
    resetWifiBt.loop();
    wifiManager.process();
    tConnectMqtt.update();
    tReconnectMqtt.update();
}