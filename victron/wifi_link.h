// =============================================================================
//  On-panel Wi-Fi setup -- the link to ESPHome's Wi-Fi component (firmware only).
//
//  wifi_ui.h never touches the radio; it calls the hooks installed here. Everything
//  below uses public ESPHome API that exists in 2026.1 and in current releases:
//    set_sta / clear_sta / init_sta / add_sta   the list of networks to join
//    start_scanning()                           what the captive portal does after it
//                                               receives credentials: it makes ESPHome
//                                               scan and join the best listed network
//    get_scan_result()                          kept with set_keep_scan_results(true)
//  While connected, a scan for the list is started on the driver directly so the
//  current connection is not torn down by ESPHome's own state machine.
// =============================================================================
#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <esp_netif.h>
#include <esp_wifi.h>

#include "esphome/core/application.h"
#include "esphome/core/preferences.h"
#include "esphome/components/wifi/wifi_component.h"

#include "wifi_ui.h"

namespace vp {
namespace wifi {
namespace link {

constexpr uint32_t PREF_KEY = 0xC1E70A11u;   // flash slot of the saved-network list

inline esphome::wifi::WiFiComponent *radio() { return esphome::wifi::global_wifi_component; }

inline bool connected() { return radio() != nullptr && radio()->is_connected(); }

inline void cur_ssid(char *out, size_t n) {
  out[0] = 0;
  wifi_ap_record_t ap;
  if (!connected() || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;
  snprintf(out, n, "%.32s", (const char *) ap.ssid);
}

inline void cur_ip(char *out, size_t n) {
  out[0] = 0;
  esp_netif_t *nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip;
  if (nif == nullptr || esp_netif_get_ip_info(nif, &ip) != ESP_OK) return;
  snprintf(out, n, "%u.%u.%u.%u", (unsigned) esp_ip4_addr1_16(&ip.ip), (unsigned) esp_ip4_addr2_16(&ip.ip),
           (unsigned) esp_ip4_addr3_16(&ip.ip), (unsigned) esp_ip4_addr4_16(&ip.ip));
}

// Ask for a scan. Results are collected by ESPHome's event handler into get_scan_result().
inline void scan() {
  auto *w = radio();
  if (w == nullptr) return;
  w->set_keep_scan_results(true);   // full, unfiltered results
  if (connected()) {
    wifi_scan_config_t cfg{};
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 100;
    cfg.scan_time.active.max = 300;
    esp_wifi_scan_start(&cfg, false);   // a failure just means "try Rescan"
  } else {
    w->start_scanning();
  }
}

template<class T> inline std::string to_std(const T &s) { return std::string(s.c_str(), s.size()); }

inline void scan_results(std::vector<Raw> &out) {
  auto *w = radio();
  if (w == nullptr) return;
  const auto &res = w->get_scan_result();
  for (const auto &r : res) {
    Raw x;
    x.ssid = to_std(r.get_ssid());
    x.rssi = r.get_rssi();
    x.secure = r.get_with_auth();
    x.hidden = r.get_is_hidden() || x.ssid.empty();
    out.push_back(x);
  }
}

inline esphome::wifi::WiFiAP ap_of(const Net &n) {
  esphome::wifi::WiFiAP ap;
  ap.set_ssid(n.ssid);
  ap.set_password(n.pass);
  ap.set_hidden(n.hidden);
  return ap;
}

// Join exactly this network (not stored). ESPHome scans, finds it and connects.
inline void try_net(const Net &n) {
  auto *w = radio();
  if (w == nullptr) return;
  w->set_keep_scan_results(true);
  w->set_sta(ap_of(n));
  w->start_scanning();
}

// Allow exactly these networks (newest first). An empty list leaves the radio with nothing to join.
inline void use_saved(const std::vector<Net> &list) {
  auto *w = radio();
  if (w == nullptr) return;
  w->set_keep_scan_results(true);
  w->clear_sta();
  w->init_sta(list.size());
  for (const Net &n : list) w->add_sta(ap_of(n));
  w->start_scanning();
}

inline esphome::ESPPreferenceObject &pref() {
  static esphome::ESPPreferenceObject p = esphome::global_preferences->make_preference<Blob>(PREF_KEY, true);
  return p;
}
inline bool load(Blob &b) { return pref().load(&b); }
inline bool store(const Blob &b) {
  if (!pref().save(&b)) return false;
  return esphome::global_preferences->sync();
}

inline void reboot() { esphome::App.safe_reboot(); }

// Install the radio and storage hooks. open/back (page navigation) are set by the generated
// display code, which owns the page scripts.
inline void install() {
  Hooks &h = hooks();
  h.scan = scan;
  h.scan_results = scan_results;
  h.try_net = try_net;
  h.use_saved = use_saved;
  h.connected = connected;
  h.cur_ssid = cur_ssid;
  h.cur_ip = cur_ip;
  h.load = load;
  h.store = store;
  h.reboot = reboot;
}

}  // namespace link
}  // namespace wifi
}  // namespace vp
