#pragma once

#include <Arduino.h>

enum MenuPage : uint8_t;

struct MenuDefinition {
  MenuPage id;
  MenuPage parent;
  uint8_t *selection;
  uint8_t (*countFn)();
  void (*renderFn)(uint8_t selection);
  void (*confirmFn)(uint8_t selection);
  void (*enterFn)();
};
