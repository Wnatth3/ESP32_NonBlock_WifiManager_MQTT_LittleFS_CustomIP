
/*
Improtant: You must to erase flash before uploade the sketch.
*/

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
#define FORMAT_LITTLEFS_IF_FAILED true

#define _DEBUG_  // Comment this line if you don't want to debug

//******************************** Variables & Objects **********************//
#define deviceName "MyESP32"

const char* filename = "/config.txt";  // Config file name

bool mqttParameter;

//----------------- esLED ---------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi Manager --------------//
// default custom static IP
char static_ip[16]  = "192.168.0.191";
char static_gw[16]  = "192.168.0.1";
char static_sn[16]  = "255.255.255.0";
char static_dns[16] = "1.1.1.1";
// MQTT parameters
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
    mqttParameter = doc["mqttParameter"];

    if (doc["ip"]) {
        strlcpy(static_ip, doc["ip"], sizeof(static_ip));
        strlcpy(static_gw, doc["gateway"], sizeof(static_gw));
        strlcpy(static_sn, doc["subnet"], sizeof(static_sn));
        strlcpy(static_dns, doc["dns"], sizeof(static_dns));
    } else {
#ifdef _DEBUG_
        Serial.println(F("No custom IP in config file"));
#endif
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
#ifdef _DEBUG_
    Serial.print(F("MQTT parameters are "));
#endif
    if (mqttParameter) {
#ifdef _DEBUG_
        Serial.println(F(" available"));
#endif
        mqtt.setCallback(handleMqttMessage);
        mqtt.setServer(mqttBroker, atoi(mqttPort));
        tConnectMqtt.start();
    } else {
#ifdef _DEBUG_
        Serial.println(F(" not available."));
#endif
    }
}

void saveParamsCallback() {
    // saveConfiguration(LittleFS, filename);
    strcpy(mqttBroker, customMqttBroker.getValue());
    strcpy(mqttPort, customMqttPort.getValue());
    strcpy(mqttUser, customMqttUser.getValue());
    strcpy(mqttPass, customMqttPass.getValue());
#ifdef _DEBUG_
    Serial.println(F("The values are updated."));
#endif

    // Delete existing file, otherwise the configuration is appended to the file
    // LittleFS.remove(filename);

    File file = LittleFS.open(filename, "w");
    if (!file) {
#ifdef _DEBUG_
        Serial.println(F("Failed to open config file for writing"));
#endif
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Set the values in the document
    doc["mqttBroker"] = mqttBroker;
    doc["mqttPort"]   = mqttPort;
    doc["mqttUser"]   = mqttUser;
    doc["mqttPass"]   = mqttPass;

    doc["ip"]      = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"]  = WiFi.subnetMask().toString();
    doc["dns"]     = WiFi.dnsIP().toString();

    if (doc["mqttBroker"] != "") {
        doc["mqttParameter"] = true;
        mqttParameter        = doc["mqttParameter"];
    }

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
#ifdef _DEBUG_
        Serial.println(F("Failed to write to file"));
#endif
    } else {
#ifdef _DEBUG_
        Serial.print(F("The configuration has been saved to "));
        Serial.println(filename);
#endif
    }

    file.close();  // Close the file

    mqttInit();
}

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
#ifdef _DEBUG_
        Serial.print((char)file.read());
#endif
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

    // set static ip
    IPAddress _ip, _gw, _sn, _dns;
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
    _dns.fromString(static_dns);
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns);

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
        Serial.println(F("WiFI is connected :D"));
#endif
    } else {
#ifdef _DEBUG_
        Serial.println(F("Configportal running"));
#endif
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
            Serial.println(F("Connected"));
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
                tReconnectMqtt.stop();
                tConnectMqtt.interval(60 * 1000);  // Wait 1 minute before reconnecting.
                tConnectMqtt.start();
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
        tConnectMqtt.stop();
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
    delay(3000);
}

//********************************  Setup ***********************************//
void setup() {
    statusLed.turnOFF();
    resetWifiBt.begin(resetWifiBtPin);
    resetWifiBt.setLongClickTime(5000);
    resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);
    Serial.begin(115200);
    // while (!Serial)
    //     continue;
    // Initialize LittleFS library
    while (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
#ifdef _DEBUG_
        Serial.println(F("Failed to initialize LittleFS library"));
#endif
        delay(1000);
    }

    wifiManagerSetup();
    mqttInit();
}

//********************************  Loop ************************************//
void loop() {
    statusLed.loop();
    resetWifiBt.loop();
    wifiManager.process();
    tConnectMqtt.update();
    tReconnectMqtt.update();
}