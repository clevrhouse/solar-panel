#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>

namespace vp {

// Session-only interaction state; a held finger must never time out.
struct DisplayWake {
  static constexpr uint32_t TIMEOUT_MS = 120000;
  bool awake = false, held = false;
  uint32_t last_touch = 0;
  void touch(bool down, uint32_t now) { awake = true; held = down; last_touch = now; }
  bool expire(uint32_t now) {
    if (!awake || held || uint32_t(now - last_touch) < TIMEOUT_MS) return false;
    awake = false;
    return true;
  }
};

inline bool ok(float value) { return std::isfinite(value); }
inline float complete_sum(float a, float b) { return ok(a) && ok(b) ? a + b : NAN; }
inline float sum3(const float v[3]) {
  return ok(v[0]) && ok(v[1]) && ok(v[2]) ? v[0] + v[1] + v[2] : NAN;
}

// Each subscribed field has its own clock. A busy grid cannot keep solar alive.
enum Channel {
  C_PV, C_YIELD, C_MPPT_V, C_MPPT_I, C_MPPT_STATE,
  C_SOC, C_BATT_V, C_BATT_P, C_BATT_I, C_TTG,
  C_GRID1, C_GRID2, C_GRID3, C_ACL1, C_ACL2, C_ACL3,
  C_CRIT1, C_CRIT2, C_CRIT3, C_INV, C_FRONIUS, C_MIN, C_ACTIVE_MIN,
  B_PV, B_YIELD, B_V, B_I, B_STATE, CHANNEL_COUNT
};
constexpr uint32_t CERBO_MAX_AGE_MS = 65000; // two ~30 s Cerbo republishes (site keepalive) + margin
struct Freshness {
  std::array<uint32_t, CHANNEL_COUNT> received{};
  std::array<bool, CHANNEL_COUNT> seen{};
  void clear() { seen.fill(false); }
  void clear_relay() { for (int i = B_PV; i <= B_STATE; ++i) seen[i] = false; }
  void touch(Channel c, uint32_t now) { received[c] = now; seen[c] = true; }
  bool fresh(Channel c, uint32_t now, uint32_t limit, bool connected) const {
    return connected && seen[c] && uint32_t(now - received[c]) <= limit;
  }
  float value(Channel c, float v, uint32_t now, bool connected) const {
    return ok(v) && fresh(c, now, CERBO_MAX_AGE_MS, connected) ? v : NAN;
  }
  bool aligned(Channel a, Channel b, uint32_t now, uint32_t limit = 10000) const {
    if (!seen[a] || !seen[b]) return false;
    uint32_t aa = now - received[a], ab = now - received[b];
    return (aa > ab ? aa - ab : ab - aa) <= limit;
  }
};

// Matches the existing D_* series order. Values and reset dates arrive in one
// HA JSON attribute, never as independent subscriptions with ambiguous order.
constexpr int METER_COUNT = 6;
struct MeterSample {
  float today = NAN, previous = NAN;
  time_t reset = 0;
};
struct EnergyFeed {
  std::array<MeterSample, METER_COUNT> meters{};
  time_t observed = 0;
  uint32_t received = 0;
  bool seen = false;
  void clear() { seen = false; }
  bool fresh(time_t now, uint32_t ms, bool connected) const {
    return connected && seen && now > 0 && observed > 0 &&
           observed <= now + 5 && now - observed <= 90 &&
           uint32_t(ms - received) <= 90000;
  }
};

inline int local_day(time_t t) {
  if (t <= 0) return 0;
  struct tm parts;
  localtime_r(&t, &parts);
  return (parts.tm_year + 1900) * 1000 + parts.tm_yday;
}
inline float meter_today(const EnergyFeed &feed, int i, time_t now,
                         uint32_t ms, bool connected) {
  const auto &m = feed.meters[i];
  return feed.fresh(now, ms, connected) && m.reset > 0 && m.reset <= feed.observed &&
         local_day(m.reset) == local_day(now) && ok(m.today) && m.today >= 0
         ? m.today : NAN;
}
// Return the day *ended by the source's reset*, not "yesterday" on the panel.
// Reject manual/midday resets: they do not establish a completed calendar day.
inline int completed_meter_day(const EnergyFeed &feed, int i, time_t now,
                               uint32_t ms, bool connected) {
  const auto &m = feed.meters[i];
  if (!feed.fresh(now, ms, connected) || !ok(m.previous) || m.previous < 0 ||
      m.reset <= 0 || m.reset > feed.observed) return 0;
  struct tm end;
  localtime_r(&m.reset, &end);
  if (end.tm_hour != 0 || end.tm_min != 0) return 0;
  --end.tm_mday;
  end.tm_hour = 12; end.tm_min = 0; end.tm_sec = 0; end.tm_isdst = -1;
  return local_day(mktime(&end));
}

// Disposable HA snapshot. Never accumulated or restored on the device.
struct HistoryFeed {
  using Row = std::array<float, METER_COUNT>;
  std::array<Row, 24> hours;
  std::array<Row, 8> days;
  std::array<time_t, 8> dates{};
  time_t observed = 0;
  uint32_t received = 0;
  bool seen = false;
  HistoryFeed() {
    for (auto &row : hours) row.fill(NAN);
    for (auto &row : days) row.fill(NAN);
  }
  bool fresh(time_t now, uint32_t ms, bool connected) const {
    return connected && seen && now > 0 && observed > 0 &&
      observed <= now + 5 && now - observed <= 180 &&
      uint32_t(ms - received) <= 180000 && local_day(now) == local_day(observed);
  }
};

} // namespace vp
