#include "WiFiManagerHandler.h"

// ── Static instance pointer (needed for the trampoline callback) ─────────────
WiFiManagerHandler* WiFiManagerHandler::_instance = nullptr;

// ────────────────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────────────────
WiFiManagerHandler::WiFiManagerHandler(const char* deviceName, const char* apPassword)
    : _deviceName(deviceName)
    , _apPassword(apPassword)
    , _paramBroker("broker", "mqtt server", _mqttBroker, 16)
    , _paramPort("port", "mqtt port", _mqttPort, 6)
    , _paramUser("user", "mqtt user", _mqttUser, 10)
    , _paramPass("pass", "mqtt pass", _mqttPass, 10) {
  // Default values
  strlcpy(_mqttBroker, "192.168.0.10", sizeof(_mqttBroker));
  strlcpy(_mqttPort, "1883", sizeof(_mqttPort));
  _mqttUser[0] = '\0';
  _mqttPass[0] = '\0';

#ifdef CUSTOM_IP
  strlcpy(_static_ip, "192.168.0.191", sizeof(_static_ip));
  strlcpy(_static_gw, "192.168.0.1", sizeof(_static_gw));
  strlcpy(_static_sn, "255.255.255.0", sizeof(_static_sn));
  strlcpy(_static_dns, "1.1.1.1", sizeof(_static_dns));
#endif

  _instance = this;  // Register singleton for trampoline
}

// ────────────────────────────────────────────────────────────────────────────
//  begin()  –  call once in setup()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::begin() {
  _loadConfig();

#ifdef _DEBUG_
  File file = LittleFS.open(CONFIG_FILENAME, "r");
  if (file) _printConfig();
#endif

#ifndef _DEBUG_
  _wm.setDebugOutput(true, WM_DEBUG_SILENT);
#endif

#ifdef CUSTOM_IP
  _applyStaticIp();
#endif

  _wm.addParameter(&_paramBroker);
  _wm.addParameter(&_paramPort);
  _wm.addParameter(&_paramUser);
  _wm.addParameter(&_paramPass);

  _wm.setHttpPort(80);
  // HTTPUpdateServer credentials are set internally by WiFiManager
  // Lock the portal itself with:
  // _wm.setAPStaticIPConfig(/*ip, gateway, subnet*/); // optional
  // For portal password, AP password already covers access
  std::vector<const char*> menu = { "wifi",  // Configure WiFi
                                    "info",  // Board info
                                    // "param",   // Custom parameters (MQTT etc.)
                                    "update",  // ← OTA upload page
                                    "sep",     // Separator
                                    "restart", "erase", "exit" };
  _wm.setMenu(menu);

  _wm.setDarkMode(true);
  _wm.setConnectTimeout(10);
  _wm.setConfigPortalTimeout(60);
  _wm.setConfigPortalBlocking(false);
  _wm.setSaveParamsCallback(_saveParamsTrampoline);

  if (_wm.autoConnect(_deviceName, _apPassword)) {
    _delnF("WiFi is connected :D");
  } else {
    _delnF("Config portal running");
  }
}

// ────────────────────────────────────────────────────────────────────────────
//  process()  –  call every loop()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::process() {
  _wm.process();
}

// ────────────────────────────────────────────────────────────────────────────
//  resetAndRestart()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::resetAndRestart() {
  _delnF("Deleting config file and resetting WiFi settings.");
  _deleteConfig();
  _wm.resetSettings();
  _delnF("Restarting...");
  delay(3000);
  ESP.restart();
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _loadConfig()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::_loadConfig() {
  _delnF("Loading configuration from file");

  File file = LittleFS.open(CONFIG_FILENAME, "r");
  if (!file) {
    _delnF("Config file not found – using defaults");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    _delnF("Failed to parse config – using defaults");
    return;
  }

  if (doc["mqttBroker"]) strlcpy(_mqttBroker, doc["mqttBroker"], sizeof(_mqttBroker));
  if (doc["mqttPort"]) strlcpy(_mqttPort, doc["mqttPort"], sizeof(_mqttPort));
  if (doc["mqttUser"]) strlcpy(_mqttUser, doc["mqttUser"], sizeof(_mqttUser));
  if (doc["mqttPass"]) strlcpy(_mqttPass, doc["mqttPass"], sizeof(_mqttPass));
  _mqttParameter = doc["mqttParameter"] | false;

#ifdef CUSTOM_IP
  if (doc["ip"]) {
    strlcpy(_static_ip, doc["ip"], sizeof(_static_ip));
    strlcpy(_static_gw, doc["gateway"], sizeof(_static_gw));
    strlcpy(_static_sn, doc["subnet"], sizeof(_static_sn));
    strlcpy(_static_dns, doc["dns"], sizeof(_static_dns));
  } else {
    _delnF("No custom IP in config – using defaults");
  }
#endif
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _saveConfig()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::_saveConfig() {
  // Pull updated values from the portal form fields
  strlcpy(_mqttBroker, _paramBroker.getValue(), sizeof(_mqttBroker));
  strlcpy(_mqttPort, _paramPort.getValue(), sizeof(_mqttPort));
  strlcpy(_mqttUser, _paramUser.getValue(), sizeof(_mqttUser));
  strlcpy(_mqttPass, _paramPass.getValue(), sizeof(_mqttPass));

  _delnF("Saving configuration to file");

  File file = LittleFS.open(CONFIG_FILENAME, "w");
  if (!file) {
    _delnF("Failed to open config file for writing");
    return;
  }

  JsonDocument doc;
  doc["mqttBroker"] = _mqttBroker;
  doc["mqttPort"]   = _mqttPort;
  doc["mqttUser"]   = _mqttUser;
  doc["mqttPass"]   = _mqttPass;

  if (strlen(_mqttBroker) > 0) {
    doc["mqttParameter"] = true;
    _mqttParameter       = true;
  }

#ifdef CUSTOM_IP
  doc["ip"]      = WiFi.localIP().toString();
  doc["gateway"] = WiFi.gatewayIP().toString();
  doc["subnet"]  = WiFi.subnetMask().toString();
  doc["dns"]     = WiFi.dnsIP().toString();
#endif

  if (serializeJson(doc, file) == 0) {
    _delnF("Failed to write config file");
  } else {
    _delnF("Configuration saved successfully");
  }

  file.close();
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _printConfig()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::_printConfig() {
  _delnF("--- Config file contents ---");

  File file = LittleFS.open(CONFIG_FILENAME, "r");
  if (!file) {
    _delnF("Config file not found");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    _delnF("Failed to parse config file for printing");
    return;
  }

  char buffer[512];
  serializeJsonPretty(doc, buffer);
  _delnF(buffer);
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _deleteConfig()
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::_deleteConfig() {
  if (LittleFS.remove(CONFIG_FILENAME)) {
    _delnF("Config file deleted");
  } else {
    _delnF("Failed to delete config file (may not exist)");
  }
}

// ────────────────────────────────────────────────────────────────────────────
//  Private – _applyStaticIp()
// ────────────────────────────────────────────────────────────────────────────
#ifdef CUSTOM_IP
void WiFiManagerHandler::_applyStaticIp() {
  IPAddress ip, gw, sn, dns;
  ip.fromString(_static_ip);
  gw.fromString(_static_gw);
  sn.fromString(_static_sn);
  dns.fromString(_static_dns);
  _wm.setSTAStaticIPConfig(ip, gw, sn, dns);
}
#endif

// ────────────────────────────────────────────────────────────────────────────
//  Static trampoline  →  routes the WiFiManager C-style callback to our member
// ────────────────────────────────────────────────────────────────────────────
void WiFiManagerHandler::_saveParamsTrampoline() {
  if (_instance) {
    _instance->_saveConfig();
    if (_instance->_onParamsSaved) { _instance->_onParamsSaved(); }
  }
}

// void WiFiManagerHandler::enableOTA() {
//   _wm.setHttpPort(80);
//   // HTTPUpdateServer credentials are set internally by WiFiManager
//   // Lock the portal itself with:
//   // _wm.setAPStaticIPConfig(/*ip, gateway, subnet*/); // optional
//   // For portal password, AP password already covers access
//   std::vector<const char*> menu = { "wifi",    // Configure WiFi
//                                     "info",    // Board info
//                                     "param",   // Custom parameters (MQTT etc.)
//                                     "update",  // ← OTA upload page
//                                     "sep",     // Separator
//                                     "restart", "erase", "exit" };
//   _wm.setMenu(menu);
// }