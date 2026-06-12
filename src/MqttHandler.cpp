#include "MqttHandler.h"
#include <WiFi.h>

MqttHandler* MqttHandler::_instance = nullptr;

// ────────────────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────────────────
MqttHandler::MqttHandler(const char* deviceName) : _deviceName(deviceName), _mqtt(_espClient) {
  _instance = this;
}

// ────────────────────────────────────────────────────────────────────────────
//  begin()
// ────────────────────────────────────────────────────────────────────────────
void MqttHandler::begin(const char* broker, uint16_t port, const char* user, const char* pass) {
  if (!broker || strlen(broker) == 0) {
    _delnF("MqttHandler: no broker address – skipping init");
    return;
  }

  _user = user;
  _pass = pass;

  _mqtt.setCallback(_messageTrampoline);
  _mqtt.setServer(broker, port);
  _initialised = true;

  _deVar("MqttHandler: broker=", broker);
  _deVar(" port=", port);
  _delnF("");
}

// ────────────────────────────────────────────────────────────────────────────
//  loop()
// ────────────────────────────────────────────────────────────────────────────
void MqttHandler::loop() {
  if (!_initialised) return;

  if (_mqtt.connected()) {
    _mqtt.loop();
    return;
  }

  // Throttle reconnect attempts
  uint32_t now = millis();
  if (now - _lastAttempt < _retryDelay) return;
  _lastAttempt = now;

  _connect();
}

// ────────────────────────────────────────────────────────────────────────────
//  publish() / subscribe()
// ────────────────────────────────────────────────────────────────────────────
bool MqttHandler::publish(const char* topic, const char* payload) {
  if (!_mqtt.connected()) return false;
  return _mqtt.publish(topic, payload);
}

bool MqttHandler::subscribe(const char* topic) {
  if (!_mqtt.connected()) return false;
  return _mqtt.subscribe(topic);
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _connect()
// ────────────────────────────────────────────────────────────────────────────
void MqttHandler::_connect() {
  if (WiFi.status() != WL_CONNECTED) {
    _delnF("MqttHandler: WiFi not connected - skipping MQTT attempt");
    return;
  }

  _deF("MqttHandler: connecting... ");

  bool ok = (_user && strlen(_user) > 0) ? _mqtt.connect(_deviceName, _user, _pass)
                                         : _mqtt.connect(_deviceName);

  if (ok) {
    _delnF("connected");
    _failCount  = 0;
    _retryDelay = 3000;

    if (_onConnected) _onConnected();
  } else {
    _failCount++;
    _deVar("failed, state=", _mqtt.state());
    _deVar(" attempt=", _failCount);
    _delnF("");

    if (_failCount >= _maxFastRetries) {
      _retryDelay = _slowRetryDelay;
      _deVar("MqttHandler: backing off for ", _slowRetryDelay / 1000);
      _delnF("s");
    }
  }
}

// ────────────────────────────────────────────────────────────────────────────
//  Static trampoline
// ────────────────────────────────────────────────────────────────────────────
void MqttHandler::_messageTrampoline(char* topic, byte* payload, unsigned int length) {
  if (!_instance || !_instance->_onMessage) return;

  String t = topic;
  String m;
  m.reserve(length);
  for (unsigned int i = 0; i < length; i++) m += (char)payload[i];

  _instance->_onMessage(t, m);
}
