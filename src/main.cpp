#include <Arduino.h>
#include <LittleFS.h>
#include <ezLED.h>
#include <TaskScheduler.h>

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

//******************************** MQTT logic *******************************//
void onMqttConnected() {
  statusLed.blinkNumberOfTimes(200, 200, 3);
  // mqttHandler.subscribe("test/subscribe/topic");
  // mqttHandler.publish("test/publish/topic", "Hello World!");
}

void onMqttMessage(const String& topic, const String& message) {
  if (topic == "test/subscribe/topic") {
    if (message == "aValue") {            /* Do something */
    } else if (message == "otherValue") { /* Do something */
    }
  }
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
    _delnF("LittleFS init failed – retrying");
    delay(1000);
  }

  wifiHandler.setOnParamsSaved(initMqtt);
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
  statusLed.loop();
  resetButton.loop();
}