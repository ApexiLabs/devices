#pragma once
#include <cstdint>
#include <cstring>

namespace QueueAge {
// Exact firmware UTC timestamp format only. Zero means unknown, not fresh.
inline uint32_t epoch(const char *value) {
  if (value == nullptr || std::strlen(value) != 20 || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':' || value[19] != 'Z') return 0;
  const int starts[]{0, 5, 8, 11, 14, 17};
  int fields[6]{};
  for (int field = 0; field < 6; ++field) {
    const int length = field == 0 ? 4 : 2;
    for (int i = 0; i < length; ++i) {
      const char digit = value[starts[field] + i];
      if (digit < '0' || digit > '9') return 0;
      fields[field] = fields[field] * 10 + digit - '0';
    }
  }
  int y = fields[0], mo = fields[1], d = fields[2];
  const int h = fields[3], mi = fields[4], s = fields[5];
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || h > 23 || mi > 59 || s > 59) return 0;
  const int days[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
  if (d < 1 || d > days[mo - 1] + (mo == 2 && leap ? 1 : 0)) return 0;
  y -= mo <= 2;
  const int era = y / 400;
  const unsigned year = y - era * 400;
  const unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = year * 365 + year / 4 - year / 100 + doy;
  return static_cast<uint32_t>((era * 146097 + static_cast<int>(doe) - 719468) * 86400LL + h * 3600 + mi * 60 + s);
}
inline bool age(uint32_t recordEpoch, int64_t now, uint32_t &seconds) {
  if (recordEpoch == 0 || now < recordEpoch || now - recordEpoch > UINT32_MAX) return false;
  seconds = static_cast<uint32_t>(now - recordEpoch);
  return true;
}
}
