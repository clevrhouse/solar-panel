// =============================================================================
//  Live snapshot decoder (schema v1)
//
//  The Clevr Victron integration in Home Assistant reads the Cerbo's MQTT feed
//  and publishes everything the panel shows live as one JSON attribute of
//  sensor.clevr_victron_live_snapshot. Nothing site specific is compiled into
//  the firmware: topology (phases, battery, AC PV, capacity) travels in "cfg".
//
//  decode_live() is a template over the JSON object type so it can be tested on
//  a host with plain ArduinoJson and used unchanged in an ESPHome lambda.
//  A malformed packet returns false and leaves the previous values to age out.
// =============================================================================
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace vp {

struct PanelCfg {
  int gm = 7, am = 7, cm = 7;   // phase masks: grid, AC input load, AC output load
  int pvn = 0;                  // AC PV inverters
  bool bat = true;              // battery installed (History shows charge/discharge)
  float cap = 10.0f;            // usable kWh
};

struct LiveFeed {
  PanelCfg cfg;
  bool mq = true;               // HA's MQTT client is connected (false: every value below is null)
  int32_t ga = -1;              // seconds since HA last heard from the GX; -1: nothing since HA started
  int64_t at = 0;               // when HA built this snapshot (epoch); 0 if absent
  float pv = NAN, yd = NAN, mv = NAN, mi = NAN;                      // MPPT: power, yield today, V, A
  float soc = NAN, bv = NAN, bp = NAN, bi = NAN, ttg = NAN;           // battery
  float pw = NAN, mn = NAN, ma = NAN;                                 // PV inverter power, ESS min SOC, active limit
  std::array<float, 3> g{{NAN, NAN, NAN}}, a{{NAN, NAN, NAN}}, c{{NAN, NAN, NAN}};
  int ms = -1, vs = -1;                                               // MPPT / VE.Bus state codes
  bool fr = false;                                                    // PV inverter is a Fronius
  float mp = NAN, ap = NAN, ao = NAN;                                 // aggregates over all devices
  std::array<float, 5> fc{{NAN, NAN, NAN, NAN, NAN}};                 // forecast: today, left, this h, next h, tomorrow
  int64_t fu = 0;                                                     // when the forecast last changed (epoch)
};

inline std::string mppt_state_name(int v) {
  switch (v) {
    case 0: return "Off"; case 2: return "Fault"; case 3: return "Bulk"; case 4: return "Absorption";
    case 5: return "Float"; case 6: return "Storage"; case 7: return "Equalize"; case 245: return "Wake-up";
    case 247: return "ESS"; case 252: return "Ext. control";
  }
  return "State " + std::to_string(v);
}

inline std::string vebus_state_name(int v) {
  switch (v) {
    case 0: return "Off"; case 1: return "Low power"; case 2: return "Fault"; case 3: return "Bulk";
    case 4: return "Absorption"; case 5: return "Float"; case 6: return "Storage"; case 7: return "Equalize";
    case 8: return "Passthru"; case 9: return "Inverting"; case 10: return "Assisting";
    case 11: return "Power supply"; case 252: return "Bulk prot.";
  }
  return "State " + std::to_string(v);
}

template <class V> inline float live_num(V v) {
  return v.template is<float>() ? v.template as<float>() : NAN;
}
template <class V> inline int live_int(V v, int fallback) {
  return v.template is<int>() ? v.template as<int>() : fallback;
}

template <class Obj> inline bool decode_live(Obj doc, LiveFeed &o) {
  if (!doc["v"].template is<int>() || doc["v"].template as<int>() != 1) return false;
  if (doc["L"].isNull() || doc["cfg"].isNull()) return false;
  auto L = doc["L"];
  auto cfg = doc["cfg"];
  PanelCfg c;
  c.gm = live_int(cfg["gm"], c.gm); c.am = live_int(cfg["am"], c.am); c.cm = live_int(cfg["cm"], c.cm);
  c.gm &= 7; c.am &= 7; c.cm &= 7;
  c.pvn = live_int(cfg["pvn"], 0);
  if (c.pvn < 0 || c.pvn > 32) c.pvn = 0;
  if (cfg["bat"].template is<bool>()) c.bat = cfg["bat"].template as<bool>();
  float cap = live_num(cfg["cap"]);
  if (cap > 0.0f && cap < 10000.0f) c.cap = cap;
  o.cfg = c;
  // Older documents carry no feed state: treat them as "connected, age unknown".
  o.mq = !(doc["mq"].template is<int>() && doc["mq"].template as<int>() == 0) &&
         !(doc["mq"].template is<bool>() && !doc["mq"].template as<bool>());
  o.ga = doc["ga"].template is<int>() && doc["ga"].template as<int>() >= 0 ? doc["ga"].template as<int>() : -1;
  o.at = doc["at"].template is<int64_t>() ? doc["at"].template as<int64_t>() : 0;
  o.pv = live_num(L["pv"]); o.yd = live_num(L["yd"]); o.mv = live_num(L["mv"]); o.mi = live_num(L["mi"]);
  o.soc = live_num(L["soc"]); o.bv = live_num(L["bv"]); o.bp = live_num(L["bp"]);
  o.bi = live_num(L["bi"]); o.ttg = live_num(L["ttg"]);
  o.pw = live_num(L["pw"]); o.mn = live_num(L["mn"]); o.ma = live_num(L["ma"]);
  for (int i = 0; i < 3; ++i) {
    o.g[i] = live_num(L["g"][i]); o.a[i] = live_num(L["a"][i]); o.c[i] = live_num(L["c"][i]);
  }
  o.ms = live_int(L["ms"], -1);
  o.vs = live_int(L["vs"], -1);
  o.fr = L["fr"].template is<bool>() && L["fr"].template as<bool>();
  o.mp = live_num(doc["H"]["mp"]); o.ap = live_num(doc["H"]["ap"]); o.ao = live_num(doc["H"]["ao"]);
  for (int i = 0; i < 5; ++i) o.fc[i] = live_num(doc["F"][i]);
  o.fu = doc["fu"].template is<int64_t>() ? doc["fu"].template as<int64_t>() : 0;
  return true;
}

}  // namespace vp
