#include <Arduino.h>
#include <LittleFS.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <ezLED.h>

//******************************** Debug ************************************//
#define _DEBUG_  // Comment out to disable all debug output
#include "Debug.h"

#include "WiFiManagerHandler.h"
#include "MqttHandler.h"
#include "ResetButton.h"

//******************************** Config ***********************************//
#define DEVICE_NAME "MyESP32"
#define AP_PASSWORD "password"
#define RESET_BTN_PIN 0
#define RESET_HOLD_MS 5000

//******************************** Objects **********************************//
Scheduler ts;

ezLED statusLed(LED_BUILTIN);

WiFiManagerHandler wifiHandler(DEVICE_NAME, AP_PASSWORD);
MqttHandler mqttHandler(DEVICE_NAME);
ResetButton resetButton(RESET_BTN_PIN, RESET_HOLD_MS);

//******************************** Tasks ************************************//
Task tWifiManager(TASK_IMMEDIATE, TASK_FOREVER, []() { wifiHandler.process(); }, &ts, true);
Task tMqtt(100, TASK_FOREVER, []() { mqttHandler.loop(); }, &ts, true);
Task tStatusLed(TASK_IMMEDIATE, TASK_FOREVER, []() { statusLed.loop(); }, &ts, true);
Task tResetButton(TASK_IMMEDIATE, TASK_FOREVER, []() { resetButton.loop(); }, &ts, true);

//******************************** MQTT logic *******************************//
void onMqttConnected() {
  statusLed.blinkNumberOfTimes(200, 200, 3);
  mqttHandler.subscribe("omg/OMG_ESP32_BLE/BTtoMQTT/A4C138C5BFA8");
  // mqttHandler.publish("test/publish/topic", "Hello World!");
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  _def("Message arrived [ %s ]", topic);
  //   for (int i = 0; i < length; i++) { _def("%c", (char)payload[i]); }
  // _def("\n");

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    _def("JSON parse failed: %s\n", error.c_str());
    return;
  }

  float tempc = doc["tempc"] | 0.0f;
  float hum   = doc["hum"] | 0.0f;

  _def(" tempc: %.2f | hum: %.2f\n", tempc, hum);
}

void initMqtt() {
  if (!wifiHandler.hasMqttParams()) return;

  mqttHandler.setOnConnected(onMqttConnected);
  mqttHandler.setOnMessage(onMqttMessage);
  mqttHandler.begin(wifiHandler.getMqttBroker(), atoi(wifiHandler.getMqttPort()),
                    wifiHandler.getMqttUser(), wifiHandler.getMqttPass());
}

//******************************** Setup & Loop *****************************//
void setup() {
  _serialBegin(115200);
  statusLed.turnOFF();

  while (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    _delnF("LittleFS init failed - retrying");
    delay(1000);
  }

  wifiHandler.setOnParamsSaved(initMqtt);
  // wifiHandler.enableOTA();
  wifiHandler.begin();
  initMqtt();

  resetButton.setOnLongPress([]() {
    statusLed.turnON();
    wifiHandler.resetAndRestart();
  });
  resetButton.begin();
}

void loop() {
  ts.execute();
  // statusLed.loop();
  // resetButton.loop();
}