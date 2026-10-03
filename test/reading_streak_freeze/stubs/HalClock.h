#pragma once

#include <cstdint>

struct HalClock {
  bool getDateTime(uint16_t&, uint8_t&, uint8_t&, uint8_t&, uint8_t&) const { return false; }
};

inline HalClock halClock;
