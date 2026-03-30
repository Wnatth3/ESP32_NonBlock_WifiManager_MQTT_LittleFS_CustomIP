#include <Arduino.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <Button2.h>
#include <ezLED.h>
#include <TaskScheduler.h>

#include "WiFiManagerHandler.h"

//******************************** Variables & Objects **********************//
//----------------- TaskScheduler ------------------//
Scheduler ts;

#define deviceName "MyESP32"
#define apPassword "password"

//----------------- LED -----------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi / MQTT ---------------//
WiFiManagerHandler wifiHandler(deviceName, apPassword);

WiFiClient   espClient;
PubSubClient mqtt(espClient);

//******************************** Tasks ************************************//
void connectMqtt();
void reconnectMqtt();
Task tWifiManager(TASK_IMMEDIATE, TASK_FOREVER, []() { wifiHandler.process(); }, &ts, true);
Task tConnectMqtt(TASK_IMMEDIATE, TASK_FOREVER, &connectMqtt, &ts, true);
Task tReconnectMqtt(3000, TASK_FOREVER, &reconnectMqtt, &ts, false);

//******************************** MQTT ************************************//
void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  if (String(topic) == "test/subscribe/topic") {
    if (message == "aValue") {            /* Do something */
    } else if (message == "otherValue") { /* Do something */
    }
  }
}

void subscribeMqtt() {
  _delnF("Subscribing to MQTT topics...");
  // mqtt.subscribe("test/subscribe/topic");
}

void publishMqtt() {
  _delnF("Publishing to MQTT topics...");
  // mqtt.publish("test/publish/topic", "Hello World!");
}

void mqttInit() {
  _deF("MQTT parameters are ");
  if (wifiHandler.hasMqttParams()) {
    _delnF("available");
    mqtt.setCallback(handleMqttMessage);
    mqtt.setServer(wifiHandler.getMqttBroker(), atoi(wifiHandler.getMqttPort()));
  } else {
    _delnF("not available.");
  }
}

void reconnectMqtt() {
  if (WiFi.status() == WL_CONNECTED) {
    _deVar("MQTT Broker: ", wifiHandler.getMqttBroker());
    _deVar(" | Port: ", wifiHandler.getMqttPort());
    _deVar(" | User: ", wifiHandler.getMqttUser());
    _deVarln(" | Pass: ", wifiHandler.getMqttPass());
    _deF("Connecting MQTT... ");

    if (mqtt.connect(deviceName, wifiHandler.getMqttUser(), wifiHandler.getMqttPass())) {
      _delnF("Connected");
      tReconnectMqtt.disable();
      tConnectMqtt.enable();
      statusLed.blinkNumberOfTimes(200, 200, 3);
      subscribeMqtt();
      publishMqtt();
    } else {
      _deVar("failed state: ", mqtt.state());
      _deVarln(" | counter: ", tReconnectMqtt.getRunCounter());
      if (tReconnectMqtt.getRunCounter() > 3) {
        tReconnectMqtt.disable();
        tConnectMqtt.setInterval(60000L);
        tConnectMqtt.enableDelayed();
      }
    }
  } else {
    if (tReconnectMqtt.isFirstIteration()) _delnF("WiFi is not connected");
  }
}

void connectMqtt() {
  if (!mqtt.connected()) {
    tConnectMqtt.disable();
    tReconnectMqtt.enable();
  } else {
    mqtt.loop();
  }
}

//----------------- Reset WiFi Button ---------//
void resetWifiBtPressed(Button2& btn) {
  statusLed.turnON();
  wifiHandler.resetAndRestart();  // Handles delete + wifiManager.resetSettings() + reboot
}

//******************************** Setup & Loop ****************************//
void setup() {
  _serialBegin(115200);
  statusLed.turnOFF();

  resetWifiBt.begin(resetWifiBtPin);
  resetWifiBt.setLongClickTime(5000);
  resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);

  while (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    _delnF("Failed to initialize LittleFS");
    delay(1000);
  }

  // Register a hook so mqttInit() runs automatically after portal saves params
  wifiHandler.setOnParamsSaved(mqttInit);

  wifiHandler.begin();
  mqttInit();
}

void loop() {
  ts.execute();
  statusLed.loop();
  resetWifiBt.loop();
}
