#include "ResetButton.h"

ResetButton* ResetButton::_instance = nullptr;

// ────────────────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────────────────
ResetButton::ResetButton(uint8_t pin, uint32_t holdMs)
    : _pin(pin), _holdMs(holdMs) {
  _instance = this;
}

// ────────────────────────────────────────────────────────────────────────────
//  begin()
// ────────────────────────────────────────────────────────────────────────────
void ResetButton::begin() {
  _btn.begin(_pin);
  _btn.setLongClickTime(_holdMs);
  _btn.setLongClickDetectedHandler(_longPressTrampoline);
  _deVar("ResetButton: armed on pin ", _pin);
  _deVar(", hold time = ", _holdMs);
  _delnF("ms");
}

// ────────────────────────────────────────────────────────────────────────────
//  loop()
// ────────────────────────────────────────────────────────────────────────────
void ResetButton::loop() {
  _btn.loop();
}

// ────────────────────────────────────────────────────────────────────────────
//  Static trampoline
// ────────────────────────────────────────────────────────────────────────────
void ResetButton::_longPressTrampoline(Button2& /*b*/) {
  if (_instance && _instance->_onLongPress) {
    _instance->_onLongPress();
  }
}
