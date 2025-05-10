#include <Arduino.h>
#include <FS.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Button2.h>
#include <ezLED.h>
#include <TickTwo.h>

//******************************** Configulation ****************************//
#define _DEBUG_  // Comment this line if you don't want to debug
#include "Debug.h"

//******************************** Variables & Objects **********************//
#define FORMAT_LITTLEFS_IF_FAILED true

#define deviceName "MyESP32"

//----------------- esLED ---------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi Manager --------------//
const char* filename = "/config.txt";  // Config file name

// default custom static IP
char static_ip[16]  = "192.168.0.191";
char static_gw[16]  = "192.168.0.1";
char static_sn[16]  = "255.255.255.0";
char static_dns[16] = "1.1.1.1";

char mqttBroker[16] = "192.168.0.10";
char mqttPort[6]    = "1883";
char mqttUser[10];
char mqttPass[10];

bool mqttParameter;

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
        _delnF("Failed to open data file");
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        _delnF("Failed to read file, using default configuration");
    }
    // Copy values from the JsonDocument to the Config
    // strlcpy(Destination_Variable, doc["Source_Variable"] /*| "Default_Value"*/, sizeof(Destination_Name));
    strlcpy(mqttBroker, doc["mqttBroker"], sizeof(mqttBroker));
    strlcpy(mqttPort, doc["mqttPort"], sizeof(mqttPort));
    strlcpy(mqttUser, doc["mqttUser"], sizeof(mqttUser));
    strlcpy(mqttPass, doc["mqttPass"], sizeof(mqttPass));
    mqttParameter = doc["mqttParameter"];

    if (doc["ip"]) {
        strlcpy(static_ip, doc["ip"], sizeof(static_ip));
        strlcpy(static_gw, doc["gateway"], sizeof(static_gw));
        strlcpy(static_sn, doc["subnet"], sizeof(static_sn));
        strlcpy(static_dns, doc["dns"], sizeof(static_dns));
    } else {
        _delnF("No custom IP in config file");
    }

    file.close();
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

void mqttInit() {
    _deF("MQTT parameters are ");
    if (mqttParameter) {
        _delnF(" available");
        mqtt.setCallback(handleMqttMessage);
        mqtt.setServer(mqttBroker, atoi(mqttPort));
        tConnectMqtt.start();
    } else {
        _delnF(" not available.");
    }
}

void saveParamsCallback() {
    // Pull the values from WiFi portal form.
    strcpy(mqttBroker, customMqttBroker.getValue());
    strcpy(mqttPort, customMqttPort.getValue());
    strcpy(mqttUser, customMqttUser.getValue());
    strcpy(mqttPass, customMqttPass.getValue());

    _delnF("The values are updated.");

    // Delete existing file, otherwise the configuration is appended to the file
    // LittleFS.remove(filename);
    File file = LittleFS.open(filename, "w");
    if (!file) {
        _delnF("Failed to open config file for writing");
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Set the values in the document
    doc["mqttBroker"] = mqttBroker;
    doc["mqttPort"]   = mqttPort;
    doc["mqttUser"]   = mqttUser;
    doc["mqttPass"]   = mqttPass;

    if (doc["mqttBroker"] != "") {
        doc["mqttParameter"] = true;
        mqttParameter        = doc["mqttParameter"];
    }

    doc["ip"]      = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"]  = WiFi.subnetMask().toString();
    doc["dns"]     = WiFi.dnsIP().toString();

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        _delnF("Failed to write to file");
    } else {
        _delnF("Configuration saved successfully");
    }

    file.close();  // Close the file

    mqttInit();
}

void printFile(fs::FS& fs, const char* filename) {
    // Open file for reading
    File file = fs.open(filename, "r");
    if (!file) {
        _delnF("Failed to open data file");
        return;
    }

    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        _delnF("Failed to read file");
    }

    char buffer[512];
    serializeJsonPretty(doc, buffer);
    _delnF(buffer);

    file.close();
}

void deleteFile(fs::FS& fs, const char* path) {
    _deVarln("Delete file: ", path);
    if (fs.remove(path)) {
        _delnF("- file deleted");
    } else {
        _delnF("- delete failed");
    }
}

void wifiManagerSetup() {
    _delnF("Loading configuration...");

    loadConfiguration(LittleFS, filename);

    // WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP

    // reset settings - wipe credentials for testing
    // wifiManager.resetSettings();

    // set static ip
    IPAddress _ip, _gw, _sn, _dns;
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
    _dns.fromString(static_dns);
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns);

    wifiManager.addParameter(&customMqttBroker);
    wifiManager.addParameter(&customMqttPort);
    wifiManager.addParameter(&customMqttUser);
    wifiManager.addParameter(&customMqttPass);

    wifiManager.setDarkMode(true);
#ifndef _DEBUG_
    wifiManager.setDebugOutput(true, WM_DEBUG_SILENT);
#endif
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setSaveParamsCallback(saveParamsCallback);

    _delnF("Print config file...");
    printFile(LittleFS, filename);

    // automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if (wifiManager.autoConnect(deviceName, "password")) {
        _delnF("WiFI is connected :D");
    } else {
        _delnF("Configportal running");
    }
}

void subscribeMqtt() {
    _delnF("Subscribing to the MQTT topics...");
    // mqtt.subscribe("test/subscribe/topic");
}

void publishMqtt() {
    _delnF("Publishing to the MQTT topics...");
    // mqtt.publish("test/publish/topic", "Hello World!");
}

//----------------- Connect MQTT --------------//
void reconnectMqtt() {
    if (WiFi.status() == WL_CONNECTED) {
        _deVar("MQTT Broker: ", mqttBroker);
        _deVar(" | Port: ", mqttPort);
        _deVar(" | User: ", mqttUser);
        _deVarln(" | Pass: ", mqttPass);
        _deF("Connecting MQTT... ");
        if (mqtt.connect(deviceName, mqttUser, mqttPass)) {
            tReconnectMqtt.stop();
            _delnF("Connected");
            tConnectMqtt.interval(0);
            tConnectMqtt.start();
            statusLed.blinkNumberOfTimes(200, 200, 3);  // 250ms ON, 750ms OFF, repeat 3 times, blink immediately
            subscribeMqtt();
            publishMqtt();
        } else {  //
            _deVar("failed state: ", mqtt.state());
            _deVarln(" | counter: ", tReconnectMqtt.counter());
            if (tReconnectMqtt.counter() >= 3) {
                tReconnectMqtt.stop();
                tConnectMqtt.interval(60 * 1000);  // Wait 1 minute before reconnecting.
                tConnectMqtt.start();
            }
        }
    } else {
        if (tReconnectMqtt.counter() <= 1) {
            _delnF("WiFi is not connected");
        }
    }
}

void connectMqtt() {
    if (!mqtt.connected()) {
        tConnectMqtt.stop();
        tReconnectMqtt.start();
    } else {
        mqtt.loop();
    }
}

//----------------- Reset WiFi Button ---------//
void resetWifiBtPressed(Button2& btn) {
    statusLed.turnON();
    _delnF("Deleting the config file and resetting WiFi.");
    deleteFile(LittleFS, filename);
    wifiManager.resetSettings();
    _deF(deviceName);
    _delnF(" is restarting.");
    delay(3000);
    ESP.restart();
}

void setup() {
    _serialBegin(115200);
    statusLed.turnOFF();
    resetWifiBt.begin(resetWifiBtPin);
    resetWifiBt.setLongClickTime(5000);
    resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);

    while (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        _delnF("Failed to initialize LittleFS library");
        delay(1000);
    }

    wifiManagerSetup();
    mqttInit();
}

void loop() {
    wifiManager.process();
    statusLed.loop();
    resetWifiBt.loop();
    tConnectMqtt.update();
    tReconnectMqtt.update();
}
