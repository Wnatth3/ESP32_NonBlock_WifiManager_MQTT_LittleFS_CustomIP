#pragma once

#include <Arduino.h>
#include <FS.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define _DEBUG_  // Comment out to disable all debug output
#include "Debug.h"

//******************************** Configulation ****************************//
#define FORMAT_LITTLEFS_IF_FAILED true

// ─── Config file ────────────────────────────────────────────────────────────
#define CONFIG_FILENAME "/config.txt"

// ─── Static IP support ──────────────────────────────────────────────────────
// #define CUSTOM_IP // Comment out to enable DHCP

class WiFiManagerHandler {
 public:
  // ── Constructor ────────────────────────────────────────────────────────────
  WiFiManagerHandler(const char* deviceName, const char* apPassword);

  // ── Lifecycle ──────────────────────────────────────────────────────────────

  /** Call once inside setup() after LittleFS.begin(). */
  void begin();

  /** Call every loop iteration (non-blocking portal processing). */
  void process();

  // ── Config access ──────────────────────────────────────────────────────────
  const char* getMqttBroker() const { return _mqttBroker; }
  const char* getMqttPort() const { return _mqttPort; }
  const char* getMqttUser() const { return _mqttUser; }
  const char* getMqttPass() const { return _mqttPass; }
  bool        hasMqttParams() const { return _mqttParameter; }

  // ── Utilities ──────────────────────────────────────────────────────────────
  void resetAndRestart();  // Wipes config file + WiFi credentials, then reboots

  // ── Callback hook ──────────────────────────────────────────────────────────
  /** Optional: called after new params are saved from the portal. */
  void setOnParamsSaved(std::function<void()> cb) { _onParamsSaved = cb; }

 private:
  // ── Portal parameters ──────────────────────────────────────────────────────
  WiFiManager          _wm;
  WiFiManagerParameter _paramBroker;
  WiFiManagerParameter _paramPort;
  WiFiManagerParameter _paramUser;
  WiFiManagerParameter _paramPass;

  // ── Runtime config values ──────────────────────────────────────────────────
  char _mqttBroker[16];
  char _mqttPort[6];
  char _mqttUser[10];
  char _mqttPass[10];
  bool _mqttParameter = false;

#ifdef CUSTOM_IP
  char _static_ip[16];
  char _static_gw[16];
  char _static_sn[16];
  char _static_dns[16];
#endif

  const char* _deviceName;
  const char* _apPassword;

  std::function<void()> _onParamsSaved = nullptr;

  // ── Private helpers ────────────────────────────────────────────────────────
  void _loadConfig();
  void _saveConfig();
  void _printConfig();
  void _deleteConfig();
  void _applyStaticIp();

  /** Static trampoline so WiFiManager can call our member saveParamsCallback. */
  static WiFiManagerHandler* _instance;
  static void                _saveParamsTrampoline();
};