#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <functional>

#define _DEBUG_  // Comment out to disable all debug output
#include "Debug.h"

class MqttHandler {
public:
  // using MessageCallback = std::function<void(const String& topic, const String& message)>;
  using MessageCallback = std::function<void(char* topic, byte* payload, unsigned int length)>;

  // ── Constructor ────────────────────────────────────────────────────────────
  MqttHandler(const char* deviceName);

  // ── Lifecycle ──────────────────────────────────────────────────────────────

  /** Configure broker and credentials, then arm the client. */
  void begin(const char* broker, uint16_t port, const char* user, const char* pass);

  /** Call every loop() – drives reconnect logic and mqtt.loop(). */
  void loop();

  // ── Pub / Sub ──────────────────────────────────────────────────────────────
  bool publish(const char* topic, const char* payload);
  bool subscribe(const char* topic);

  // ── State ──────────────────────────────────────────────────────────────────
  bool isConnected() { return _mqtt.connected(); }
  bool isInitialised() const { return _initialised; }

  // ── Callbacks ──────────────────────────────────────────────────────────────

  /** Called once right after a successful broker connection. */
  void setOnConnected(std::function<void()> cb) { _onConnected = cb; }

  /** Called for every incoming message. */
  void setOnMessage(MessageCallback cb) { _onMessage = cb; }

private:
  const char* _deviceName;
  const char* _user = nullptr;
  const char* _pass = nullptr;
  bool _initialised = false;

  WiFiClient _espClient;
  PubSubClient _mqtt;

  uint8_t _failCount                        = 0;
  uint32_t _lastAttempt                     = 0;
  uint32_t _retryDelay                      = 3000;  // ms between reconnect tries
  static constexpr uint8_t _maxFastRetries  = 3;
  static constexpr uint32_t _slowRetryDelay = 60000UL;

  std::function<void()> _onConnected = nullptr;
  MessageCallback _onMessage         = nullptr;

  // ── Private helpers ────────────────────────────────────────────────────────
  void _connect();

  /** Static trampoline for PubSubClient's C-style message callback. */
  static MqttHandler* _instance;
  static void _messageTrampoline(char* topic, byte* payload, unsigned int length);
};