#pragma once

#include <Arduino.h>
#include <Button2.h>
#include <functional>

#define _DEBUG_  // Comment out to disable all debug output
#include "Debug.h"

class ResetButton {
 public:
  // ── Constructor ────────────────────────────────────────────────────────────

  /**
   * @param pin          GPIO pin the button is wired to.
   * @param holdMs       How long (ms) the button must be held to fire. Default 5000.
   */
  ResetButton(uint8_t pin, uint32_t holdMs = 5000);

  // ── Lifecycle ──────────────────────────────────────────────────────────────
  void begin();
  void loop();

  // ── Callback ──────────────────────────────────────────────────────────────
  /** Fired when the button has been held for holdMs. */
  void setOnLongPress(std::function<void()> cb) { _onLongPress = cb; }

 private:
  uint8_t  _pin;
  uint32_t _holdMs;
  Button2  _btn;

  std::function<void()> _onLongPress = nullptr;

  static ResetButton* _instance;
  static void         _longPressTrampoline(Button2& b);
};