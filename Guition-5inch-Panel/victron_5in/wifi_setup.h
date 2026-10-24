// =============================================================================
//  On-panel Wi-Fi setup: data rules (no LVGL, no ESPHome; host-tested).
//
//  The panel keeps up to three Wi-Fi networks, newest first:
//    - a new network becomes slot 1, the others move down; a 4th drops the oldest;
//    - saving a name that is already stored updates its password and moves it to slot 1;
//    - a network is stored only after the panel really joined it (see wifi_ui.h).
//  The list is persisted as one fixed-size blob with a CRC.
// =============================================================================
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace vp {
namespace wifi {

constexpr int MAX_SAVED = 3;
constexpr size_t SSID_MAX = 32, PASS_MIN = 8, PASS_MAX = 63;
constexpr uint32_t CONNECT_TIMEOUT_MS = 30000;   // joining a network, including DHCP
constexpr uint32_t SCAN_SETTLE_MS = 6000;        // a scan needs about 2-3 s; results are re-read until then

enum Last : uint8_t { L_UNKNOWN = 0, L_OK = 1, L_FAILED = 2, L_NOT_FOUND = 3 };

struct Net {
  std::string ssid, pass;
  bool hidden = false;
  uint8_t last = L_UNKNOWN;   // how the last attempt ended
};

inline bool valid_ssid(const std::string &s) { return !s.empty() && s.size() <= SSID_MAX; }
// Open networks have no password; secured ones need 8..63 characters (WPA).
inline bool valid_pass(const std::string &p) { return p.empty() || (p.size() >= PASS_MIN && p.size() <= PASS_MAX); }

struct Saved {
  std::vector<Net> n;

  int find(const std::string &ssid) const {
    for (size_t i = 0; i < n.size(); i++)
      if (n[i].ssid == ssid) return (int) i;
    return -1;
  }
  // Rules in the header comment. Returns false for an invalid name or password.
  bool add(const std::string &ssid, const std::string &pass, bool hidden = false) {
    if (!valid_ssid(ssid) || !valid_pass(pass)) return false;
    int i = find(ssid);
    if (i >= 0) n.erase(n.begin() + i);
    Net x;
    x.ssid = ssid; x.pass = pass; x.hidden = hidden; x.last = L_OK;
    n.insert(n.begin(), x);
    if ((int) n.size() > MAX_SAVED) n.resize(MAX_SAVED);
    return true;
  }
  bool forget(int i) {
    if (i < 0 || i >= (int) n.size()) return false;
    n.erase(n.begin() + i);
    return true;
  }
  void mark(const std::string &ssid, uint8_t last) {
    int i = find(ssid);
    if (i >= 0) n[i].last = last;
  }
};

// ---------------------------------------------------------------- persistence
struct Blob {
  uint8_t version, count, pad0, pad1;   // plain data: stored as is
  struct Entry {
    char ssid[SSID_MAX + 1];
    char pass[PASS_MAX + 1];
    uint8_t hidden, last, pad0, pad1;
  } e[MAX_SAVED];
  uint32_t crc;
};

inline uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}
inline uint32_t blob_crc(const Blob &b) { return crc32((const uint8_t *) &b, offsetof(Blob, crc)); }

inline Blob pack(const Saved &s) {
  Blob b{};
  b.version = 1;
  b.count = (uint8_t) std::min<size_t>(s.n.size(), MAX_SAVED);
  for (int i = 0; i < b.count; i++) {
    strncpy(b.e[i].ssid, s.n[i].ssid.c_str(), SSID_MAX);
    strncpy(b.e[i].pass, s.n[i].pass.c_str(), PASS_MAX);
    b.e[i].hidden = s.n[i].hidden ? 1 : 0;
    b.e[i].last = s.n[i].last;
  }
  b.crc = blob_crc(b);
  return b;
}
inline bool unpack(const Blob &b, Saved &out) {
  out.n.clear();
  if (b.version != 1 || b.count > MAX_SAVED || b.crc != blob_crc(b)) return false;
  for (int i = 0; i < b.count; i++) {
    Net x;
    x.ssid.assign(b.e[i].ssid, strnlen(b.e[i].ssid, SSID_MAX));
    x.pass.assign(b.e[i].pass, strnlen(b.e[i].pass, PASS_MAX));
    x.hidden = b.e[i].hidden != 0;
    x.last = b.e[i].last;
    if (!valid_ssid(x.ssid) || !valid_pass(x.pass)) { out.n.clear(); return false; }
    out.n.push_back(x);
  }
  return true;
}
inline bool same(const Blob &a, const Blob &b) { return memcmp(&a, &b, sizeof(Blob)) == 0; }

// ---------------------------------------------------------------- scan list
struct Raw { std::string ssid; int rssi = -100; bool secure = true, hidden = false; };
struct Found { std::string ssid; int rssi = -100; bool secure = true; };

// One row per network name (the strongest access point), strongest first. Hidden or
// empty names are dropped (a hidden network is added by typing its name).
inline std::vector<Found> clean_scan(const std::vector<Raw> &raw, size_t cap = 24) {
  std::vector<Found> out;
  for (const Raw &r : raw) {
    if (r.hidden || r.ssid.empty() || r.ssid.size() > SSID_MAX) continue;
    bool seen = false;
    for (Found &f : out) {
      if (f.ssid != r.ssid) continue;
      seen = true;
      if (r.rssi > f.rssi) { f.rssi = r.rssi; f.secure = r.secure; }
      break;
    }
    if (!seen) out.push_back({r.ssid, r.rssi, r.secure});
  }
  std::stable_sort(out.begin(), out.end(), [](const Found &a, const Found &b) { return a.rssi > b.rssi; });
  if (out.size() > cap) out.resize(cap);
  return out;
}
inline int bars(int rssi) { return rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1; }

// ---------------------------------------------------------------- connection attempt
enum class Res { NONE, CONNECTING, OK, NOT_FOUND, FAILED };
struct Attempt {
  bool active = false, hidden = false, seen = false;   // seen: the name showed up in a scan
  std::string ssid, pass;
  uint32_t t0 = 0;
};
// Decide how an attempt stands. `cur_ssid` is the network the radio is joined to now.
inline Res judge(const Attempt &a, bool connected, const std::string &cur_ssid, uint32_t now_ms) {
  if (!a.active) return Res::NONE;
  if (connected && cur_ssid == a.ssid) return Res::OK;
  if (now_ms - a.t0 < CONNECT_TIMEOUT_MS) return Res::CONNECTING;
  // ESPHome does not report why joining failed. A network that was never seen in a scan is
  // out of range (or misspelled); one that was seen but not joined most likely has a wrong password.
  return (a.seen || a.hidden) ? Res::FAILED : Res::NOT_FOUND;
}

}  // namespace wifi
}  // namespace vp
