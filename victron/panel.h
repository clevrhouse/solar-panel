// =============================================================================
//  Victron GX Panel -- rendering helpers (LVGL 8 and 9)
//
//  All screen logic lives here: units, colours, flow arrows, stale dimming,
//  energy accounting, history, charts. The YAML side only collects values
//  and calls render functions once a second.
//
//  Depends only on LVGL and the C/C++ standard library, so it can be
//  compile-checked on its own against the LVGL headers.
//
//  Widget pointers come from ui.h, which is GENERATED together with the
//  display YAML -- do not edit ui.h by hand.
// =============================================================================
#pragma once
#include <array>
#include <algorithm>
#include <utility>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "lvgl.h"
#include "ui.h"
#include "data_state.h"
#include "live_feed.h"
#include "nav_slots.h"

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace vp {

// ESPHome renamed the HA-only connection check in newer releases.
template<class Server>
inline auto ha_connected(Server *s, int) -> decltype(s->is_connected_with_state_subscription(), bool()) {
  return s != nullptr && s->is_connected_with_state_subscription();
}
template<class Server>
inline auto ha_connected(Server *s, long) -> decltype(s->is_connected(true), bool()) {
  return s != nullptr && s->is_connected(true);
}


// ESPHome 2026.1 uses LVGL 8.4; newer releases use LVGL 9.
#if LVGL_VERSION_MAJOR == 8
using lv_point_precise_t = lv_point_t;
inline bool lv_color_eq(lv_color_t a, lv_color_t b) { return a.full == b.full; }
inline void lv_obj_remove_flag(lv_obj_t *o, lv_obj_flag_t f) { lv_obj_clear_flag(o, f); }
inline void lv_obj_remove_state(lv_obj_t *o, lv_state_t s) { lv_obj_clear_state(o, s); }
inline lv_indev_t *lv_indev_active() { return lv_indev_get_act(); }
#endif


// ---------------------------------------------------------------- palette
// From the design's style sheet. BATT is new: the design used a green that
// clashed with grid export (design correction #4).
enum : uint32_t {
  BG = 0x0A0E14, CARD = 0x141A23, RAISED = 0x1E2733, DIVIDER = 0x2A3441,
  TXT = 0xE8EDF3, TXT2 = 0x9AA6B5, TXT3 = 0x5E6A7B, MUTED = 0x7D8898,
  IMPORT = 0xF0524D, EXPORT = 0x25A35E, ACL = 0xA5D63F, CRIT = 0x22B8CF,
  BATT = 0xA78BFA, SOLAR = 0xF5B82E, HOME = 0xB8C4D2, ACCENT = 0x2F80ED,
  WARN = 0xFF9F43, OK_BG = 0x10301E, WARN_BG = 0x3A2A0E, BAD_BG = 0x3A1414,
  SOLAR_DIM = 0x4A3A12,
};

constexpr float FLOW_W = 20.0f;  // below this a flow counts as idle

// ---------------------------------------------------------------- primitives
inline bool &stale() { static bool s = false; return s; }

inline void text(lv_obj_t *o, const char *s) {
  if (o == nullptr) return;
  const char *cur = lv_label_get_text(o);
  if (cur != nullptr && strcmp(cur, s) == 0) return;  // no redraw if unchanged
  lv_label_set_text(o, s);
}
inline void textf(lv_obj_t *o, const char *fmt, ...) {
  char b[128];
  va_list a;
  va_start(a, fmt);
  vsnprintf(b, sizeof b, fmt, a);
  va_end(a);
  text(o, b);
}
// Text colour. Values dim to grey while data is stale; labels that are not
// data (captions, units in grey) pass dimmable=false.
inline void color(lv_obj_t *o, uint32_t c, bool dimmable = true) {
  if (o == nullptr) return;
  if (dimmable && stale()) c = TXT3;
  lv_color_t n = lv_color_hex(c);
  if (lv_color_eq(lv_obj_get_style_text_color(o, LV_PART_MAIN), n)) return;
  lv_obj_set_style_text_color(o, n, LV_PART_MAIN);
}
inline void bg(lv_obj_t *o, uint32_t c) {
  if (o == nullptr) return;
  lv_color_t n = lv_color_hex(c);
  if (lv_color_eq(lv_obj_get_style_bg_color(o, LV_PART_MAIN), n)) return;
  lv_obj_set_style_bg_color(o, n, LV_PART_MAIN);
}
inline void show(lv_obj_t *o, bool on) {
  if (o == nullptr) return;
  if (on == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
inline void set_w(lv_obj_t *o, int w) {
  if (o == nullptr) return;
  if (w < 0) w = 0;
  show(o, w > 0);
  if (lv_obj_get_width(o) != w) lv_obj_set_width(o, w);
}
inline void set_x(lv_obj_t *o, int x) {
  if (o != nullptr && lv_obj_get_x(o) != x) lv_obj_set_x(o, x);
}
// A small badge: label with its own background.
inline void badge(lv_obj_t *o, const char *s, uint32_t fg, uint32_t bgc) {
  text(o, s);
  color(o, fg, false);
  if (o != nullptr) {
    lv_color_t n = lv_color_hex(bgc);
    if (!lv_color_eq(lv_obj_get_style_bg_color(o, LV_PART_MAIN), n))
      lv_obj_set_style_bg_color(o, n, LV_PART_MAIN);
  }
}
inline void hhmm(char *b, size_t n, time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(b, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}
inline void ago(char *b, size_t n, uint32_t secs) {
  if (secs < 60) snprintf(b, n, "%u s ago", (unsigned) secs);
  else if (secs < 3600) snprintf(b, n, "%u min ago", (unsigned) (secs / 60));
  else snprintf(b, n, "%uh %um ago", (unsigned) (secs / 3600), (unsigned) (secs % 3600 / 60));
}

// ---------------------------------------------------------------- power units
// One unit per card (design correction #5): if the card total is 1 kW or
// more, every number on the card is in kW; otherwise all in W.
inline bool kw_for(float ref) { return ok(ref) && fabsf(ref) >= 1000.0f; }
inline void pw(char *b, size_t n, float w, bool kw, bool sign) {
  if (!ok(w)) { snprintf(b, n, "---"); return; }
  if (kw) snprintf(b, n, sign ? "%+.2f" : "%.2f", w / 1000.0f);
  else snprintf(b, n, sign ? "%+.0f" : "%.0f", w);
}
inline uint32_t grid_color(float w) {
  if (!ok(w)) return TXT3;
  return w > 0 ? IMPORT : (w < 0 ? EXPORT : TXT2);
}

// Total (value + unit) and three phases, all in the card's unit.
inline void power_card(lv_obj_t *val, lv_obj_t *unit, lv_obj_t *const ph[3], float total,
                       const float p[3], bool sign, uint32_t col, bool grid_phases) {
  bool kw = kw_for(total);
  char b[24];
  pw(b, sizeof b, total, kw, sign);
  text(val, b);
  text(unit, ok(total) ? (kw ? "kW" : "W") : "");
  color(val, ok(total) ? col : TXT3);
  color(unit, ok(total) ? col : TXT3);
  for (int i = 0; i < 3; i++) {
    // Phases share the card's unit; in kW they get one decimal so three fit
    // side by side (the per-phase overlay shows full watts).
    if (kw && ok(p[i])) snprintf(b, sizeof b, sign ? "%+.1f" : "%.1f", p[i] / 1000.0f);
    else pw(b, sizeof b, p[i], kw, sign);
    text(ph[i], b);
    color(ph[i], !ok(p[i]) ? TXT3 : (grid_phases ? grid_color(p[i]) : TXT));
  }
}

// ---------------------------------------------------------------- flow lines
inline const char *arrow_sym(char d) {
  switch (d) {
    case 'L': return LV_SYMBOL_LEFT;
    case 'R': return LV_SYMBOL_RIGHT;
    case 'U': return LV_SYMBOL_UP;
    default: return LV_SYMBOL_DOWN;
  }
}
// Segments turn their colour when power flows, grey when idle or stale; the
// arrow points `pos` for positive power and `neg` for negative.
inline void flow(lv_obj_t *const seg[], int n, lv_obj_t *arrow, float w, char pos, char neg,
                 uint32_t col) {
  bool on = ok(w) && fabsf(w) > FLOW_W && !stale();
  for (int i = 0; i < n; i++) bg(seg[i], on ? col : DIVIDER);
  if (arrow != nullptr) {
    show(arrow, on);
    if (on) {
      text(arrow, arrow_sym(w > 0 ? pos : neg));
      color(arrow, col, false);
    }
  }
}

// ---------------------------------------------------------------- data
inline float phase_sum(const float *values, unsigned mask) {
  float total = 0;
  for (int i=0; i<3; ++i) if (mask & (1u<<i)) {
    if (!ok(values[i])) return NAN;
    total += values[i];
  }
  return total;
}

struct NowData {
  float grid[3] = {NAN, NAN, NAN}, acl[3] = {NAN, NAN, NAN}, crit[3] = {NAN, NAN, NAN};
  float soc = NAN, batt_w = NAN, batt_v = NAN, batt_a = NAN, ttg_s = NAN;
  float minsoc = NAN, minsoc_active = NAN;
  unsigned grid_mask = 7, acl_mask = 7, crit_mask = 7;
  float fr_out_w = NAN;
  float fr_w = NAN, mp_w = NAN, fr_kwh = NAN, mp_kwh = NAN;
  bool pvi_fronius = false;  // Cerbo publishes pvinverter/<n>/FroniusDeviceType
  int pvi_count = 1;
  const char *inv = "";
  float cap_kwh = 10.0f;
  time_t now = 0;
};

inline void battery_eta(char *b, size_t n, const NowData &d, float grid_w) {
  b[0] = 0;
  if (!ok(d.soc) || !ok(d.batt_w)) return;
  if (d.batt_w > FLOW_W && d.soc < 99.5f && d.now > 0) {
    float hours = (100.0f - d.soc) / 100.0f * d.cap_kwh / (d.batt_w / 1000.0f);
    if (hours < 24.0f) {
      char t[8];
      hhmm(t, sizeof t, d.now + (time_t) (hours * 3600.0f));
      snprintf(b, n, "Full ~%s", t);
    }
  } else if (d.batt_w < -FLOW_W && ok(d.ttg_s) && d.ttg_s > 0 && d.ttg_s < 86400 && d.now > 0) {
    char t[8];
    hhmm(t, sizeof t, d.now + (time_t) d.ttg_s);
    // Victron TimeToGo targets the monitor's discharge floor, not ESS min SOC.
    snprintf(b, n, "Monitor ~%s", t);
  } else if (fabsf(d.batt_w) <= FLOW_W && ok(grid_w) && grid_w > FLOW_W) {
    snprintf(b, n, "Grid assisting");
  }
}

inline void battery_state(lv_obj_t *o, float w) {
  if (!ok(w)) { text(o, "---"); color(o, TXT3); return; }
  if (w > FLOW_W) { text(o, "CHARGING"); color(o, BATT); }
  else if (w < -FLOW_W) { text(o, "DISCHARGING"); color(o, BATT); }
  else { text(o, "IDLE"); color(o, TXT2); }
}

// ---------------------------------------------------------------- NOW
inline void render_now(const NowData &d) {
  UI &u = ui();
  char b[48];
  float grid = phase_sum(d.grid, d.grid_mask), acl = phase_sum(d.acl, d.acl_mask), crit = phase_sum(d.crit, d.crit_mask);

  // grid card
  lv_obj_t *gph[3] = {u.grid_l1, u.grid_l2, u.grid_l3};
  power_card(u.grid_total, u.grid_unit, gph, grid, d.grid, true, grid_color(grid), true);
  if (!ok(grid)) { text(u.grid_state, ""); }
  else if (grid > 0) { text(u.grid_state, "IMPORTING"); color(u.grid_state, IMPORT); }
  else if (grid < 0) { text(u.grid_state, "EXPORTING"); color(u.grid_state, EXPORT); }
  else { text(u.grid_state, ""); }

  lv_obj_t *aph[3] = {u.acl_l1, u.acl_l2, u.acl_l3};
  power_card(u.acl_total, u.acl_unit, aph, acl, d.acl, false, ACL, false);
  lv_obj_t *cph[3] = {u.crit_l1, u.crit_l2, u.crit_l3};
  power_card(u.crit_total, u.crit_unit, cph, crit, d.crit, false, CRIT, false);

  lv_obj_t *gc[3] = {u.grid_cap1, u.grid_cap2, u.grid_cap3};
  lv_obj_t *ac[3] = {u.acl_cap1, u.acl_cap2, u.acl_cap3};
  lv_obj_t *cc[3] = {u.crit_cap1, u.crit_cap2, u.crit_cap3};
  for (int i=0; i<3; ++i) {
    show(gc[i], d.grid_mask & (1u<<i)); show(gph[i], d.grid_mask & (1u<<i));
    show(ac[i], d.acl_mask & (1u<<i)); show(aph[i], d.acl_mask & (1u<<i));
    show(cc[i], d.crit_mask & (1u<<i)); show(cph[i], d.crit_mask & (1u<<i));
  }

  // inverter
  text(u.inv_state, (d.inv != nullptr && d.inv[0]) ? d.inv : "---");
  color(u.inv_state, TXT);

  // flows. Topology (design correction #1): grid and AC loads on the AC-in
  // side; critical loads AND the Fronius on the AC-out side; battery and
  // MPPT on the DC side. The AC-out segment next to the MultiPlus carries
  // critical minus Fronius: positive = MultiPlus supplies AC-out, negative =
  // Fronius surplus flowing back into the MultiPlus.
  lv_obj_t *fg[2] = {u.fl_grid_a, u.fl_grid_b};
  flow(fg, 2, u.ar_grid, grid, 'R', 'L', grid_color(grid));
  lv_obj_t *fa[2] = {u.fl_acl_a, u.fl_acl_b};
  flow(fa, 2, u.ar_acl, acl, 'R', 'L', ACL);
  float out = (ok(crit) && ok(d.fr_out_w)) ? crit - d.fr_out_w : NAN;
  lv_obj_t *fo[1] = {u.fl_out};
  flow(fo, 1, u.ar_out, out, 'R', 'L', CRIT);
  lv_obj_t *fc[1] = {u.fl_crit};
  flow(fc, 1, nullptr, crit, 'U', 'U', CRIT);
  lv_obj_t *ff[2] = {u.fl_fr_h, u.fl_fr_v};
  flow(ff, 2, u.ar_fr, d.fr_out_w, 'U', 'D', SOLAR);
  lv_obj_t *fb[1] = {u.fl_batt};
  flow(fb, 1, u.ar_batt, d.batt_w, 'D', 'U', BATT);
  lv_obj_t *fm[1] = {u.fl_mppt};
  flow(fm, 1, u.ar_mppt, d.mp_w, 'U', 'D', SOLAR);

  // battery
  battery_state(u.batt_state, d.batt_w);
  if (ok(d.soc)) textf(u.batt_soc, "%.0f", d.soc); else text(u.batt_soc, "---");
  color(u.batt_soc, TXT); color(u.batt_pct, TXT);
  bool bkw = kw_for(d.batt_w);
  pw(b, sizeof b, d.batt_w, bkw, true);
  text(u.batt_pw, b);
  text(u.batt_pw_u, ok(d.batt_w) ? (bkw ? "kW" : "W") : "");
  uint32_t bc = ok(d.batt_w) && fabsf(d.batt_w) > FLOW_W ? BATT : MUTED;
  color(u.batt_pw, bc); color(u.batt_pw_u, bc);
  battery_eta(b, sizeof b, d, grid);
  text(u.batt_eta, b);
  set_w(u.batt_bar, ok(d.soc) ? (int) lroundf(202.0f * d.soc / 100.0f) : 0);
  bg(u.batt_bar, stale() ? TXT3 : BATT);
  show(u.batt_min_mk, ok(d.minsoc));
  if (ok(d.minsoc)) set_x(u.batt_min_mk, 20 + (int) lroundf(202.0f * d.minsoc / 100.0f) - 1);
  if (ok(d.batt_v) && ok(d.batt_a)) textf(u.batt_va, "%.2f V   %+.1f A", d.batt_v, d.batt_a);
  else text(u.batt_va, "---");
  if (ok(d.minsoc)) textf(u.batt_minsoc, "MIN %.0f %%", d.minsoc);  // correction #9
  else text(u.batt_minsoc, "MIN --- %");

  // solar card: both arrays, one unit for the whole card
  float tot = complete_sum(d.fr_w, d.mp_w);
  bool skw = kw_for(tot);
  pw(b, sizeof b, tot, skw, false);
  text(u.solar_total, b);
  text(u.solar_unit, ok(tot) ? (skw ? "kW" : "W") : "");
  uint32_t sc = ok(tot) && tot > FLOW_W ? SOLAR : MUTED;
  color(u.solar_total, sc); color(u.solar_unit, sc);
  float today = complete_sum(d.fr_kwh, d.mp_kwh);
  if (ok(today)) textf(u.solar_today, "%.2f kWh today", today); else text(u.solar_today, "--.-- kWh today");
  text(u.fr_name, d.pvi_count > 1 ? "AC Solar" : (d.pvi_fronius ? "Fronius" : "PV Inverter"));
  // A reviewed system with no AC PV has a valid zero contribution to solar
  // totals, but should not display a phantom inverter row at 0 W.
  bool has_ac_pv = d.pvi_count > 0;
  show(u.fr_name, has_ac_pv); show(u.fr_w, has_ac_pv); show(u.fr_kwh, has_ac_pv);
  const struct { lv_obj_t *w, *k; float pw_, kwh; } rows[2] = {
      {u.fr_w, u.fr_kwh, d.fr_w, d.fr_kwh}, {u.mp_w, u.mp_kwh, d.mp_w, d.mp_kwh}};
  for (auto &r : rows) {
    char v[16];
    pw(v, sizeof v, r.pw_, skw, false);
    if (ok(r.pw_)) textf(r.w, "%s %s", v, skw ? "kW" : "W"); else text(r.w, "---");
    color(r.w, ok(r.pw_) && r.pw_ > FLOW_W ? SOLAR : MUTED);
    if (ok(r.kwh)) textf(r.k, "%.2f kWh", r.kwh); else text(r.k, "");
  }
}

// ---------------------------------------------------------------- per-phase overlay
inline void render_phase(const NowData &d) {
  UI &u = ui();
  lv_obj_t *st[4] = {u.ph_l1_state, u.ph_l2_state, u.ph_l3_state, u.ph_t_state};
  lv_obj_t *gv[4] = {u.ph_l1_grid, u.ph_l2_grid, u.ph_l3_grid, u.ph_t_grid};
  lv_obj_t *av[4] = {u.ph_l1_acl, u.ph_l2_acl, u.ph_l3_acl, u.ph_t_acl};
  lv_obj_t *cv[4] = {u.ph_l1_crit, u.ph_l2_crit, u.ph_l3_crit, u.ph_t_crit};
  lv_obj_t *bar[3] = {u.ph_l1_bar, u.ph_l2_bar, u.ph_l3_bar};
  float g[4] = {d.grid[0], d.grid[1], d.grid[2], phase_sum(d.grid, d.grid_mask)};
  float a[4] = {d.acl[0], d.acl[1], d.acl[2], phase_sum(d.acl, d.acl_mask)};
  float c[4] = {d.crit[0], d.crit[1], d.crit[2], phase_sum(d.crit, d.crit_mask)};
  const float scale = 2500.0f;  // W for half the bar
  for (int i = 0; i < 4; i++) {
    char b[24];
    pw(b, sizeof b, g[i], false, true);
    if (ok(g[i])) textf(gv[i], "%s W", b); else text(gv[i], "---");
    color(gv[i], grid_color(g[i]));
    if (ok(g[i]) && g[i] > 0) { text(st[i], "IMPORT"); color(st[i], IMPORT); }
    else if (ok(g[i]) && g[i] < 0) { text(st[i], "EXPORT"); color(st[i], EXPORT); }
    else text(st[i], "");
    if (ok(a[i])) textf(av[i], "%.0f W", a[i]); else text(av[i], "---");
    if (ok(c[i])) textf(cv[i], "%.0f W", c[i]); else text(cv[i], "---");
    color(av[i], ok(a[i]) ? ACL : TXT3); color(cv[i], ok(c[i]) ? CRIT : TXT3);
    if (i < 3) {  // bar grows from the centre (x=176): right = import, left = export
      float v = ok(g[i]) ? g[i] : 0;
      int px = (int) lroundf(fminf(fabsf(v), scale) / scale * 96.0f);
      set_w(bar[i], px);
      set_x(bar[i], v >= 0 ? 176 : 176 - px);
      bg(bar[i], v >= 0 ? IMPORT : EXPORT);
    }
  }
  float cmax = -1e9f, cmin = 1e9f;
  for (int i = 0; i < 3; i++)
    if ((d.crit_mask & (1u<<i)) && ok(d.crit[i])) { cmax = fmaxf(cmax, d.crit[i]); cmin = fminf(cmin, d.crit[i]); }
  if (d.crit_mask && ok(phase_sum(d.crit, d.crit_mask))) textf(u.ph_imb, "Critical loads imbalance %.0f W   Bar scale 2.5 kW", cmax - cmin);
  else text(u.ph_imb, "Bar scale 2.5 kW");
}

// ---------------------------------------------------------------- Min SOC overlay
inline void render_minsoc(const NowData &d, int pending, bool writable) {
  UI &u = ui();
  char b[48];
  battery_state(u.ms_state, d.batt_w);
  if (ok(d.soc)) textf(u.ms_soc, "%.0f", d.soc); else text(u.ms_soc, "---");
  bool kw = kw_for(d.batt_w);
  pw(b, sizeof b, d.batt_w, kw, true);
  text(u.ms_pw, b);
  text(u.ms_pw_u, ok(d.batt_w) ? (kw ? "kW" : "W") : "");
  if (ok(d.batt_v) && ok(d.batt_a)) textf(u.ms_va, "%.2f V   %+.1f A", d.batt_v, d.batt_a);
  else text(u.ms_va, "");
  battery_eta(b, sizeof b, d, phase_sum(d.grid, d.grid_mask));
  text(u.ms_note, b);
  set_w(u.ms_bar, ok(d.soc) ? (int) lroundf(448.0f * d.soc / 100.0f) : 0);
  bg(u.ms_bar, BATT);
  show(u.ms_mk_cur, ok(d.minsoc));
  if (ok(d.minsoc)) set_x(u.ms_mk_cur, 16 + (int) lroundf(448.0f * d.minsoc / 100.0f) - 1);
  set_x(u.ms_mk_new, 16 + (int) lroundf(448.0f * pending / 100.0f) - 2);
  textf(u.ms_val, "%d", pending);
  if (ok(d.minsoc)) textf(u.ms_was, "was %.0f%%", d.minsoc); else text(u.ms_was, "");
  if (ok(d.minsoc_active)) textf(u.ms_active, "ACTIVE LIMIT %.0f%%", d.minsoc_active);
  else text(u.ms_active, "");
  if (lv_slider_get_value(u.ms_slider) != pending) lv_slider_set_value(u.ms_slider, pending, LV_ANIM_OFF);
  text(u.ms_help, writable
       ? "Below this level the battery stops discharging and the grid supplies the loads. Written to the Cerbo on SET."
       : "Read-only on this panel. Writing is disabled in victron-gx-panel.yaml (minsoc_write).");
  bg(u.ms_set, writable ? ACCENT : RAISED);
  color(u.ms_set_lbl, writable ? 0xFFFFFF : TXT3, false);
}

// ---------------------------------------------------------------- energy accounting
// Home Assistant's energy-dashboard method: home is derived, so conversion
// and battery losses are folded into "home" and in/out balance exactly.
struct Energy {
  float solar = NAN, imp = NAN, exp = NAN, bin = NAN, bout = NAN;
  float home() const {
    if (!ok(solar) || !ok(imp) || !ok(exp) || !ok(bin) || !ok(bout)) return NAN;
    float h = solar + imp + bout - exp - bin;
    return h < 0 ? 0 : h;
  }
};

inline void stack3(lv_obj_t *const seg[3], const float v[3], int x0, int w) {
  float t = 0;
  for (int i = 0; i < 3; i++) if (ok(v[i]) && v[i] > 0) t += v[i];
  int x = x0;
  const int gap = 2;
  for (int i = 0; i < 3; i++) {
    int sw = 0;
    if (t > 0 && ok(v[i]) && v[i] > 0) sw = (int) lroundf((w - 2 * gap) * v[i] / t);
    set_x(seg[i], x);
    set_w(seg[i], sw);
    if (sw > 0) x += sw + gap;
  }
}

inline void kwh(lv_obj_t *o, float v) {
  if (ok(v)) textf(o, "%.2f", v); else text(o, "---");
}

// ---------------------------------------------------------------- TODAY
inline void render_today(const Energy &e) {
  UI &u = ui();
  float home = e.home();
  float ss = (ok(home) && home > 0.01f && ok(e.imp)) ? 100.0f * (1.0f - e.imp / home) : NAN;
  if (ok(ss)) { ss = fmaxf(0, fminf(100, ss)); textf(u.td_selfsuff, "%.0f", ss); }
  else text(u.td_selfsuff, "--");
  lv_arc_set_value(u.td_arc, ok(ss) ? (int) lroundf(ss) : 0);
  kwh(u.td_home, home);
  if (ok(e.solar)) textf(u.td_solar, "%.2f kWh", e.solar); else text(u.td_solar, "--- kWh");
  if (ok(e.imp) && ok(e.exp)) {
    float net = e.imp - e.exp;
    textf(u.td_gridnet, "%+.2f kWh %s", net, net < 0 ? "EXPORT" : "IMPORT");
    color(u.td_gridnet, net < 0 ? EXPORT : IMPORT);
  } else { text(u.td_gridnet, "---"); color(u.td_gridnet, TXT3); }

  float in[3] = {e.solar, e.bout, e.imp};
  float out[3] = {home, e.bin, e.exp};
  float tin = (ok(e.solar) && ok(e.bout) && ok(e.imp)) ? e.solar + e.bout + e.imp : NAN;
  float tout = (ok(home) && ok(e.bin) && ok(e.exp)) ? home + e.bin + e.exp : NAN;
  if (ok(tin)) textf(u.in_total, "%.2f kWh", tin); else text(u.in_total, "--- kWh");
  if (ok(tout)) textf(u.out_total, "%.2f kWh", tout); else text(u.out_total, "--- kWh");
  kwh(u.in_solar, e.solar); kwh(u.in_batt, e.bout); kwh(u.in_grid, e.imp);
  kwh(u.out_home, home); kwh(u.out_batt, e.bin); kwh(u.out_export, e.exp);
  lv_obj_t *si[3] = {u.in_seg0, u.in_seg1, u.in_seg2};
  lv_obj_t *so[3] = {u.out_seg0, u.out_seg1, u.out_seg2};
  stack3(si, in, 24, 432);
  stack3(so, out, 24, 432);
}

// HA owns all history; these helpers only format its current RAM snapshot.
enum { D_MPPT = 0, D_FRON, D_IMP, D_EXP, D_BIN, D_BOUT, D_N };
inline int day_key(time_t t) { return local_day(t); }
inline Energy history_energy(const HistoryFeed::Row &r) {
  Energy e;
  e.solar = complete_sum(r[D_MPPT], r[D_FRON]);
  e.imp = r[D_IMP]; e.exp = r[D_EXP];
  e.bin = r[D_BIN]; e.bout = r[D_BOUT];
  return e;
}
inline float solar_avg7(const HistoryFeed &f) {
  float sum = 0; int count = 0;
  for (int i = 0; i < 7; ++i) {
    float solar = history_energy(f.days[i]).solar;
    if (ok(solar)) { sum += solar; ++count; }
  }
  return count ? sum / count : NAN;
}

// ---------------------------------------------------------------- history chart
#include "history_4in.h"

// ---------------------------------------------------------------- FORECAST
struct FcData {
  float fc_today = NAN, fc_left = NAN, this_h = NAN, next_h = NAN, tomorrow = NAN;
  float produced = NAN, yesterday = NAN, avg7 = NAN;
  float soc = NAN, batt_w = NAN, cap_kwh = 10.0f;
  time_t now = 0;
};

inline void fc_bar(lv_obj_t *fill, lv_obj_t *tick, int x0, int w, float v, float ref, float full) {
  // fill = v/full of the width; tick marks ref/full
  set_w(fill, (ok(v) && full > 0) ? (int) lroundf(fminf(v / full, 1.0f) * w) : 0);
  show(tick, ok(ref) && full > 0);
  if (ok(ref) && full > 0) set_x(tick, x0 + (int) lroundf(fminf(ref / full, 1.0f) * w) - 1);
}

inline void rating(lv_obj_t *o, float v, float avg) {
  if (!ok(v) || !ok(avg) || avg <= 0) { text(o, ""); return; }
  float r = v / avg;
  if (r >= 1.15f) { text(o, "STRONG"); color(o, SOLAR, false); }
  else if (r <= 0.7f) { text(o, "WEAK"); color(o, TXT2, false); }
  else { text(o, "AVERAGE"); color(o, TXT2, false); }
}

inline void render_forecast(const FcData &f) {
  UI &u = ui();
  if (ok(f.fc_today)) textf(u.fc_forecast, "FORECAST %.1f kWh", f.fc_today);
  else text(u.fc_forecast, "FORECAST --- kWh");
  kwh(u.fc_prod, f.produced);
  if (ok(f.fc_left)) textf(u.fc_left, "%.1f", f.fc_left); else text(u.fc_left, "---");
  // Bar scaled to the larger of forecast and produced+left. The tick marks
  // what the forecast expected by now (forecast minus what it says is left).
  float full = ok(f.fc_today) ? f.fc_today : NAN;
  if (ok(f.produced) && ok(f.fc_left) && ok(full)) full = fmaxf(full, f.produced + f.fc_left);
  float expected = (ok(f.fc_today) && ok(f.fc_left)) ? f.fc_today - f.fc_left : NAN;
  fc_bar(u.fc_bar, u.fc_tick, 24, 432, f.produced, expected, full);
  if (ok(f.produced) && ok(expected)) {
    float d = f.produced - expected;
    textf(u.fc_vs, "%.1f kWh %s", fabsf(d), d >= 0 ? "ahead" : "under");
    color(u.fc_vs, d >= 0 ? EXPORT : WARN);
  } else { text(u.fc_vs, "---"); color(u.fc_vs, TXT3); }
  if (ok(f.this_h)) textf(u.fc_this, "%.1f kWh", f.this_h); else text(u.fc_this, "---");
  if (ok(f.next_h)) textf(u.fc_next, "%.1f kWh", f.next_h); else text(u.fc_next, "---");

  // tomorrow
  static const char *dn[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  static const char *ds[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (f.now > 0) {
    time_t t = f.now + 86400;
    struct tm tm;
    localtime_r(&t, &tm);
    textf(u.fc_d1_name, "TOMORROW, %s", ds[tm.tm_wday]);
    t = f.now - 86400;
    localtime_r(&t, &tm);
    textf(u.fc_d2_name, "YESTERDAY, %s", ds[tm.tm_wday]);
  }
  (void) dn;
  if (ok(f.tomorrow)) textf(u.fc_d1_kwh, "%.1f", f.tomorrow); else text(u.fc_d1_kwh, "---");
  if (ok(f.yesterday)) textf(u.fc_d2_kwh, "%.1f", f.yesterday); else text(u.fc_d2_kwh, "---");
  rating(u.fc_d1_rating, f.tomorrow, f.avg7);
  text(u.fc_d2_rating, "ACTUAL");
  color(u.fc_d2_rating, TXT2, false);
  // Bars: full scale = 2x the 7-day average; the tick marks the average.
  float full7 = ok(f.avg7) ? 2.0f * f.avg7 : NAN;
  fc_bar(u.fc_d1_bar, u.fc_d1_tick, 22, 200, f.tomorrow, f.avg7, full7);
  fc_bar(u.fc_d2_bar, u.fc_d2_tick, 258, 200, f.yesterday, f.avg7, full7);
  if (ok(f.tomorrow) && ok(f.avg7) && f.avg7 > 0) textf(u.fc_d1_avg, "%.0f%% of 7-day avg", 100.0f * f.tomorrow / f.avg7);
  else text(u.fc_d1_avg, "7-day avg building up");
  if (ok(f.yesterday) && ok(f.avg7) && f.avg7 > 0) textf(u.fc_d2_avg, "%.0f%% of 7-day avg", 100.0f * f.yesterday / f.avg7);
  else text(u.fc_d2_avg, "");

  // Insight: facts only (design correction #10).
  char t1[64], t2[160];
  if (ok(f.soc) && ok(f.batt_w) && f.batt_w > FLOW_W && f.soc < 99.5f && f.now > 0) {
    float hours = (100.0f - f.soc) / 100.0f * f.cap_kwh / (f.batt_w / 1000.0f);
    char hm[8];
    hhmm(hm, sizeof hm, f.now + (time_t) (hours * 3600.0f));
    snprintf(t1, sizeof t1, "Battery full by about %s", hours < 24 ? hm : "--:--");
  } else if (ok(f.tomorrow)) {
    snprintf(t1, sizeof t1, "Tomorrow: %.1f kWh expected", f.tomorrow);
  } else {
    snprintf(t1, sizeof t1, "Forecast unavailable");
  }
  if (ok(f.fc_left) && ok(f.tomorrow))
    snprintf(t2, sizeof t2, "%.1f kWh still expected today. Tomorrow %.1f kWh%s.", f.fc_left, f.tomorrow,
             (ok(f.avg7) && f.avg7 > 0) ? "" : "");
  else snprintf(t2, sizeof t2, "Waiting for forecast data from Home Assistant.");
  if (ok(f.tomorrow) && ok(f.avg7) && f.avg7 > 0) {
    size_t l = strlen(t2);
    if (l > 0 && t2[l - 1] == '.') t2[l - 1] = 0;
    snprintf(t2 + strlen(t2), sizeof t2 - strlen(t2), ", %.0f%% of the 7-day average.", 100.0f * f.tomorrow / f.avg7);
  }
  text(u.fc_ins_title, t1);
  text(u.fc_ins_text, t2);
}

// ---------------------------------------------------------------- chrome
// GX feed chain: Cerbo -> MQTT broker -> Home Assistant -> ESPHome API -> panel.
// Each link reports its own state, so the badge names the link that broke:
//   !ha            panel lost Home Assistant           "HA DISCONNECTED"
//   !received      connected, first snapshot pending   "GX CONNECTING"
//   !mqtt          HA lost its MQTT connection         "GX FEED DISCONNECTED"
//   !mqtt_seen     MQTT up, nothing from the GX yet    "GX NO DATA"
//   age > STALE_S  the Cerbo stopped publishing        "GX DATA STALE <n>S"
struct Chrome {
  time_t now = 0;
  float rssi = NAN;
  bool wifi = false, ha = false;
  bool received = true;     // a live snapshot has arrived from HA since the API connected
  bool mqtt = false;        // HA reports its MQTT client connected (snapshot field mq)
  uint32_t mqtt_age_s = 0;  // age of the newest GX message, advanced by the panel's own clock
  bool mqtt_seen = false;   // HA has heard from the GX since it started (snapshot field ga >= 0)
};
constexpr uint32_t STALE_S = CERBO_MAX_AGE_MS / 1000;

inline void render_chrome(const Chrome &c) {
  UI &u = ui();
  char b[28];
  if (c.now > 0) { hhmm(b, sizeof b, c.now); text(u.sb_clock, b); } else text(u.sb_clock, "--:--");
  color(u.conn_wifi, !c.wifi ? IMPORT : (ok(c.rssi) && c.rssi < -75 ? WARN : TXT3), false);
  color(u.conn_ha, c.ha ? TXT3 : IMPORT, false);
  bool st = !c.ha || !c.received || !c.mqtt || !c.mqtt_seen || c.mqtt_age_s > STALE_S;
  stale() = st;
  if (!c.ha) {
    badge(u.conn_mqtt, "HA DISCONNECTED", IMPORT, BAD_BG);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_COVER, 0);
  } else if (!c.received) {
    badge(u.conn_mqtt, "GX CONNECTING", TXT2, RAISED);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_COVER, 0);
  } else if (!c.mqtt) {
    badge(u.conn_mqtt, "GX FEED DISCONNECTED", IMPORT, BAD_BG);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_COVER, 0);
  } else if (!c.mqtt_seen) {
    badge(u.conn_mqtt, "GX NO DATA", WARN, WARN_BG);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_COVER, 0);
  } else if (st) {
    snprintf(b, sizeof b, "GX DATA STALE %uS", (unsigned) c.mqtt_age_s);
    badge(u.conn_mqtt, b, WARN, WARN_BG);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_COVER, 0);
  } else {
    text(u.conn_mqtt, "GX");
    color(u.conn_mqtt, TXT3, false);
    lv_obj_set_style_bg_opa(u.conn_mqtt, LV_OPA_TRANSP, 0);
  }
}

// `page` is the page code from nav_slots.h: 0..3 NOW/TODAY/HISTORY/FORECAST are the main
// pages; every other code is a settings-style page without the bottom bar / side rail.
inline void render_nav(int page) {
  UI &u = ui();
  // Page load events can fire during LVGL setup, before ui_register runs.
  if (u.navbar == nullptr) return;
  lv_obj_t *bgs[4] = {u.nav_bg0, u.nav_bg1, u.nav_bg2, u.nav_bg3};
  lv_obj_t *lb[4] = {u.nav_lbl0, u.nav_lbl1, u.nav_lbl2, u.nav_lbl3};
  lv_obj_t *ul[4] = {u.nav_ul0, u.nav_ul1, u.nav_ul2, u.nav_ul3};
  // Overlays live on the top layer and otherwise survive every page change.
  show(u.ov_phase, false);
  show(u.ov_minsoc, false);
  bool settings = !is_main_page(page);
  show(u.navbar, !settings);
  show(u.sb_clock, !settings);
  show(u.set_title, settings);
  show(u.btn_settings, !settings);
  show(u.ico_settings, !settings);
  show(u.btn_close, settings);
  show(u.ico_close, settings);
  for (int i = 0; i < 4; i++) {
    int slot_page = nav_page(i);
    bool a = slot_page == page;
    text(lb[i], page_name(slot_page));
    if (bgs[i] != nullptr) lv_obj_set_style_bg_opa(bgs[i], a ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    color(lb[i], a ? TXT : TXT2, false);
    show(ul[i], a);
  }
}

// ---------------------------------------------------------------- settings
inline void ok_badge(lv_obj_t *o, bool good, const char *yes, const char *no) {
  if (good) badge(o, yes, EXPORT, OK_BG); else badge(o, no, IMPORT, BAD_BG);
}

struct NetData {
  float rssi = NAN;
  const char *ip = "";
  bool ha = false, mqtt = false, relay = false;
  uint32_t ha_up_s = 0, mqtt_age_s = 0, relay_age_s = 0;
  bool mqtt_seen = false, relay_seen = false;
  bool received = true;     // see Chrome
  time_t vrm_updated = 0;
  bool wifi_up = false, wifi_saved = false;   // radio joined / a network is stored on the panel
};
inline void render_net(const NetData &n) {
  UI &u = ui();
  char b[40];
  // Nothing saved and not connected: the status rows mean nothing; offer only "Add Wi-Fi".
  bool empty = !n.wifi_saved && !n.wifi_up;
  show(u.net_rows, !empty);
  show(u.net_empty_ic, empty);
  show(u.net_empty_t, empty);
  show(u.net_empty_s, empty);
  if (ok(n.rssi)) {
    textf(u.net_wifi, "%.0f dBm", n.rssi);
    if (n.rssi >= -60) badge(u.net_wifi_badge, "GOOD", EXPORT, OK_BG);
    else if (n.rssi >= -72) badge(u.net_wifi_badge, "FAIR", WARN, WARN_BG);
    else badge(u.net_wifi_badge, "POOR", IMPORT, BAD_BG);
  } else { text(u.net_wifi, "---"); badge(u.net_wifi_badge, "DOWN", IMPORT, BAD_BG); }
  text(u.net_ip, (n.ip && n.ip[0]) ? n.ip : "---");
  ok_badge(u.net_ha_badge, n.ha, "OK", "DOWN");
  if (n.ha) textf(u.net_ha, "Up %uh %um", (unsigned) (n.ha_up_s / 3600), (unsigned) (n.ha_up_s % 3600 / 60));
  else text(u.net_ha, "Not connected");
  bool fresh = n.mqtt && n.mqtt_seen && n.mqtt_age_s <= STALE_S;
  if (!n.ha) badge(u.net_mqtt_badge, "NO HA", IMPORT, BAD_BG);
  else if (!n.received) badge(u.net_mqtt_badge, "WAIT", TXT2, RAISED);
  else if (!n.mqtt) badge(u.net_mqtt_badge, "FEED DOWN", IMPORT, BAD_BG);
  else if (!fresh) badge(u.net_mqtt_badge, "STALE", WARN, WARN_BG);
  else badge(u.net_mqtt_badge, "OK", EXPORT, OK_BG);
  if (n.ha && n.received && n.mqtt_seen) { ago(b, sizeof b, n.mqtt_age_s); textf(u.net_mqtt, "Last data %s", b); }
  else text(u.net_mqtt, "No data yet");
  ok_badge(u.net_relay_badge, n.relay, "ONLINE", "OFFLINE");
  if (n.relay_seen) { ago(b, sizeof b, n.relay_age_s); textf(u.net_relay, "Seen %s", b); }
  else text(u.net_relay, "Never seen");
  if (n.vrm_updated > 0) { hhmm(b, sizeof b, n.vrm_updated); textf(u.net_vrm, "Updated %s", b); }
  else text(u.net_vrm, "No data yet");
}

struct RecData {
  const char *state = "";
  bool relay = false, relay_seen = false;
  uint32_t relay_age_s = 0;
  float pv_b = NAN, pv_c = NAN, y_b = NAN, y_c = NAN, bv_b = NAN, bv_c = NAN, ba_b = NAN, ba_c = NAN;
  int reset_b_min = -1, reset_c_min = -1;  // minutes after midnight, -1 = not seen
  bool fresh_b[4] = {}, fresh_c[4] = {}, aligned[4] = {};
};
inline void rec_row(lv_obj_t *vb, lv_obj_t *vc, lv_obj_t *vd, float b, float c, const char *fmt,
                    const char *dfmt, bool fresh_b, bool fresh_c, bool aligned) {
  if (ok(b)) textf(vb, fmt, b); else text(vb, "---");
  if (ok(c)) textf(vc, fmt, c); else text(vc, "---");
  color(vb, fresh_b && ok(b) ? TXT : TXT3, false);
  color(vc, fresh_c && ok(c) ? TXT : TXT3, false);
  bool valid_delta = fresh_b && fresh_c && aligned && ok(b) && ok(c);
  if (valid_delta) textf(vd, dfmt, b - c); else text(vd, "--");
  color(vd, valid_delta ? TXT2 : TXT3, false);
}
inline void render_rec(const RecData &r) {
  UI &u = ui();
  char b[40];
  textf(u.rec_state, "Charger state %s", (r.state && r.state[0]) ? r.state : "---");
  ok_badge(u.rec_relay, r.relay, "RELAY ONLINE", "RELAY OFFLINE");
  if (r.relay_seen) { ago(b, sizeof b, r.relay_age_s); textf(u.rec_seen, "Last seen %s", b); }
  else text(u.rec_seen, "Never seen");
  rec_row(u.rec_pv_b, u.rec_pv_c, u.rec_pv_d, r.pv_b, r.pv_c, "%.0f W", "%+.0f W", r.fresh_b[0], r.fresh_c[0], r.aligned[0]);
  rec_row(u.rec_y_b, u.rec_y_c, u.rec_y_d, r.y_b, r.y_c, "%.2f", "%+.2f", r.fresh_b[1], r.fresh_c[1], r.aligned[1]);
  rec_row(u.rec_bv_b, u.rec_bv_c, u.rec_bv_d, r.bv_b, r.bv_c, "%.2f V", "%+.2f V", r.fresh_b[2], r.fresh_c[2], r.aligned[2]);
  rec_row(u.rec_ba_b, u.rec_ba_c, u.rec_ba_d, r.ba_b, r.ba_c, "%.1f A", "%+.1f A", r.fresh_b[3], r.fresh_c[3], r.aligned[3]);
  // Yield reset (design correction #2): the MPPT resets on its own solar
  // day, not at midnight, so the difference is hours, not seconds.
  if (r.reset_b_min >= 0) textf(u.rec_rs_b, "%02d:%02d", r.reset_b_min / 60, r.reset_b_min % 60);
  else text(u.rec_rs_b, "--:--");
  if (r.reset_c_min >= 0) textf(u.rec_rs_c, "%02d:%02d", r.reset_c_min / 60, r.reset_c_min % 60);
  else text(u.rec_rs_c, "--:--");
  if (r.reset_b_min >= 0 && r.reset_c_min >= 0) {
    int d = r.reset_b_min - r.reset_c_min;
    if (d > 720) d -= 1440;
    if (d < -720) d += 1440;
    int a = d < 0 ? -d : d;
    textf(u.rec_rs_d, "%s%dh %02dm", d < 0 ? "-" : "+", a / 60, a % 60);
  } else text(u.rec_rs_d, "--");
}

inline void render_disp(int bright, bool night, int level, int from_min, int until_min) {
  UI &u = ui();
  textf(u.disp_bright_val, "%d%%", bright);
  if (lv_slider_get_value(u.disp_bright) != bright) lv_slider_set_value(u.disp_bright, bright, LV_ANIM_OFF);
  if (lv_obj_has_state(u.disp_night, LV_STATE_CHECKED) != night) {
    if (night) lv_obj_add_state(u.disp_night, LV_STATE_CHECKED);
    else lv_obj_remove_state(u.disp_night, LV_STATE_CHECKED);
  }
  textf(u.disp_lvl, "%d%%", level);
  textf(u.disp_from, "%02d:%02d", from_min / 60, from_min % 60);
  textf(u.disp_until, "%02d:%02d", until_min / 60, until_min % 60);
}
// Target backlight 0..100 for the current time.
inline int backlight_target(time_t now, int bright, bool night, int level, int from_min, int until_min, bool awake = false) {
  if (awake || !night || now <= 0) return bright;
  struct tm tm;
  localtime_r(&now, &tm);
  int m = tm.tm_hour * 60 + tm.tm_min;
  bool in = from_min <= until_min ? (m >= from_min && m < until_min) : (m >= from_min || m < until_min);
  return in ? level : bright;
}

inline uint32_t free_psram() {
#ifdef ESP_PLATFORM
  return (uint32_t) heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  return 0;
#endif
}
inline void render_dev(const char *fw, const char *esphome, uint32_t uptime_s, const char *board) {
  UI &u = ui();
  text(u.dev_fw, fw);
  text(u.dev_esph, esphome);
  textf(u.dev_up, "%ud %02u:%02u", (unsigned) (uptime_s / 86400), (unsigned) (uptime_s % 86400 / 3600),
        (unsigned) (uptime_s % 3600 / 60));
  textf(u.dev_psram, "%.1f MB", free_psram() / 1048576.0f);
  text(u.dev_board, board);
}

}  // namespace vp
