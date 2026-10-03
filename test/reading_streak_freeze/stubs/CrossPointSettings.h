#pragma once

#include <cstdint>

struct CrossPointSettings {
  uint8_t clockUtcOffsetQ = 48;
};

inline CrossPointSettings SETTINGS;
