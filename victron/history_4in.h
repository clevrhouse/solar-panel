#pragma once
// Four-inch History: hourly/daily views of the 5-inch battery-aware design.
// Reads only HA's disposable history snapshot; no device-side accumulation.
// Included by panel.h after Energy and history_energy have been declared.
struct HistInput {
  const HistoryFeed *feed;
  bool valid;
  time_t now;
  bool has_battery = true;
};

struct HistBars4 {
  lv_obj_t *solar = nullptr, *discharge = nullptr, *import = nullptr;
  lv_obj_t *charge = nullptr, *exported = nullptr;
};
struct Hist4 {
  bool built = false;
  bool days_mode = false;
  int hour = -1, day = 6;
  lv_obj_t *hour_sel = nullptr, *day_sel = nullptr;
  lv_obj_t *hour_zero = nullptr, *day_zero = nullptr;
  HistBars4 hours[24]{}, days[7]{};
};
inline Hist4 &hist4() { static Hist4 h; return h; }
constexpr int HIST4_CHART_TOP = 192;
constexpr int HIST4_CHART_HEIGHT = 172;
constexpr int HIST4_BAR_HEIGHT = 142;

inline lv_obj_t *hist_rect4(lv_obj_t *parent, uint32_t col, int radius = 2) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(o, 0, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  return o;
}

inline void hist_shape4(lv_obj_t *o, int x, int y, int w, int height) {
  if (o == nullptr) return;
  if (w <= 0 || height <= 0) { show(o, false); return; }
  if (lv_obj_get_x(o) != x || lv_obj_get_y(o) != y) lv_obj_set_pos(o, x, y);
  if (lv_obj_get_width(o) != w || lv_obj_get_height(o) != height)
    lv_obj_set_size(o, w, height);
  show(o, true);
}

inline void hist_build4(bool battery) {
  Hist4 &h = hist4();
  if (h.built || ui().hh_chart == nullptr || ui().hd_chart == nullptr) return;
  h.hour_sel = hist_rect4(ui().hh_chart, RAISED, 4);
  h.day_sel = hist_rect4(ui().hd_chart, RAISED, 4);
  for (int mode = 0; mode < 2; ++mode) {
    lv_obj_t *parent = mode ? ui().hd_chart : ui().hh_chart;
    HistBars4 *bars = mode ? h.days : h.hours;
    int count = mode ? 7 : 24;
    for (int i = 0; i < count; ++i) {
      bars[i].solar = hist_rect4(parent, SOLAR);
      bars[i].discharge = hist_rect4(parent, BATT);
      bars[i].import = hist_rect4(parent, IMPORT);
      bars[i].charge = hist_rect4(parent, BATT);
      bars[i].exported = hist_rect4(parent, EXPORT);
    }
  }
  // Put bars and grid lines above the selection highlight.
  for (int mode = 0; mode < 2; ++mode) {
    lv_obj_t *parent = mode ? ui().hd_chart : ui().hh_chart;
    int width = 384;
    for (int y : {4, HIST4_CHART_HEIGHT - 4})
      hist_shape4(hist_rect4(parent, DIVIDER, 0), 0, y, width, 1);
  }
  h.hour_zero = hist_rect4(ui().hh_chart, TXT3, 0);
  h.day_zero = hist_rect4(ui().hd_chart, TXT3, 0);
  h.built = true;
}

inline float hist_nice4(float value) {
  if (!ok(value) || value <= 0) return 0;
  float p = powf(10.0f, floorf(log10f(value)));
  float n = value / p;
  return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * p;
}

inline float hist_source4(const Energy &e) {
  if (!ok(e.solar) || !ok(e.imp) || !ok(e.bout)) return NAN;
  return fmaxf(0, e.solar) + fmaxf(0, e.bout) + fmaxf(0, e.imp);
}
inline float hist_sink4(const Energy &e) {
  if (!ok(e.bin) || !ok(e.exp)) return NAN;
  return fmaxf(0, e.bin) + fmaxf(0, e.exp);
}

inline void hist_legend4(bool battery) {
  UI &u = ui();
  for (lv_obj_t *o : {u.hh_leg_batt, u.hh_leg_batt_lbl, u.hd_leg_batt, u.hd_leg_batt_lbl})
    show(o, battery);
  // Close the gap in the legend when this installation has no battery.
  for (auto pair : {std::pair<lv_obj_t *, int>{u.hh_leg_imp, 24 + (battery ? 208 : 104)},
                    {u.hh_leg_exp, 24 + (battery ? 312 : 208)},
                    {u.hd_leg_imp, 24 + (battery ? 208 : 104)},
                    {u.hd_leg_exp, 24 + (battery ? 312 : 208)}}) {
    if (lv_obj_get_x(pair.first) != pair.second) lv_obj_set_x(pair.first, pair.second);
  }
  for (auto pair : {std::pair<lv_obj_t *, int>{u.hh_leg_imp_lbl, 36 + (battery ? 208 : 104)},
                    {u.hh_leg_exp_lbl, 36 + (battery ? 312 : 208)},
                    {u.hd_leg_imp_lbl, 36 + (battery ? 208 : 104)},
                    {u.hd_leg_exp_lbl, 36 + (battery ? 312 : 208)}}) {
    if (lv_obj_get_x(pair.first) != pair.second) lv_obj_set_x(pair.first, pair.second);
  }
}

inline void hist_slot4(HistBars4 &b, int x, int width, const Energy &e,
                       int zero, float positive_scale, float negative_scale,
                       int positive_height, int negative_height, bool battery) {
  float top = 0;
  const float pos[3] = {e.solar, battery ? e.bout : NAN, e.imp};
  lv_obj_t *objects[3] = {b.solar, b.discharge, b.import};
  for (int i = 0; i < 3; ++i) {
    float v = pos[i];
    if (!ok(v) || v <= 0 || positive_scale <= 0) { show(objects[i], false); continue; }
    int start = (int) lroundf(top / positive_scale * positive_height);
    top += v;
    int end = (int) lroundf(fminf(top, positive_scale) / positive_scale * positive_height);
    hist_shape4(objects[i], x, zero - end, width, end - start);
  }
  float bottom = 0;
  const float neg[2] = {battery ? e.bin : NAN, e.exp};
  lv_obj_t *sinks[2] = {b.charge, b.exported};
  for (int i = 0; i < 2; ++i) {
    float v = neg[i];
    if (!ok(v) || v <= 0 || negative_scale <= 0) { show(sinks[i], false); continue; }
    int start = (int) lroundf(bottom / negative_scale * negative_height);
    bottom += v;
    int end = (int) lroundf(fminf(bottom, negative_scale) / negative_scale * negative_height);
    hist_shape4(sinks[i], x, zero + 1 + start, width, end - start);
  }
}

inline void hist_value4(lv_obj_t *label, float value, uint32_t col, bool sign = false) {
  // Retain small real energy values (e.g. 0.04 kWh) in the numeric readout
  // even when their bar is smaller than one pixel at a seven-day scale.
  if (ok(value)) {
    if (value != 0 && fabsf(value) < 0.01f) textf(label, sign ? "%+.3f" : "%.3f", value);
    else if (value != 0 && fabsf(value) < 0.1f) textf(label, sign ? "%+.2f" : "%.2f", value);
    else textf(label, sign ? "%+.1f" : "%.1f", value);
  }
  else text(label, "--");
  color(label, col);
}

inline void hist_chart4(bool days, const Energy *values, int count, int selected,
                        float max_source, float max_sink, bool battery) {
  UI &u = ui(); Hist4 &h = hist4();
  float positive_scale = hist_nice4(max_source);
  float negative_scale = hist_nice4(max_sink);
  int positive_height = positive_scale <= 0 ? 128 :
      negative_scale <= 0 ? HIST4_BAR_HEIGHT :
      std::max(58, std::min(115, (int) lroundf(HIST4_BAR_HEIGHT * positive_scale /
                                             (positive_scale + negative_scale))));
  int negative_height = HIST4_BAR_HEIGHT - positive_height;
  int zero = 8 + positive_height;
  HistBars4 *bars = days ? h.days : h.hours;
  const int slot = days ? 54 : 16, width = days ? 28 : 10;
  hist_shape4(days ? h.day_sel : h.hour_sel, selected * slot, 0, slot, HIST4_CHART_HEIGHT);
  hist_shape4(days ? h.day_zero : h.hour_zero, 0, zero, 384, 1);
  lv_obj_t *zero_label = days ? u.hd_zero : u.hh_zero;
  if (lv_obj_get_y(zero_label) != HIST4_CHART_TOP + zero - 8)
    lv_obj_set_y(zero_label, HIST4_CHART_TOP + zero - 8);
  hist_value4(days ? u.hd_scale : u.hh_scale, positive_scale, TXT3);
  hist_value4(days ? u.hd_nscale : u.hh_nscale, negative_scale, TXT3);
  for (int i = 0; i < count; ++i)
    hist_slot4(bars[i], i * slot + (slot - width) / 2, width, values[i], zero,
               positive_scale, negative_scale, positive_height, negative_height, battery);
}

inline void render_hist(const HistInput &in) {
  if (in.now <= 0) return;
  hist_build4(in.has_battery);
  Hist4 &h = hist4(); UI &u = ui();
  if (!h.built) return;
  show(u.hh_panel, !h.days_mode); show(u.hd_panel, h.days_mode);
  bg(u.hist_seg_today, h.days_mode ? CARD : RAISED);
  bg(u.hist_seg_days, h.days_mode ? RAISED : CARD);
  color(u.hist_lbl_today, h.days_mode ? TXT2 : TXT, false);
  color(u.hist_lbl_days, h.days_mode ? TXT : TXT2, false);
  hist_legend4(in.has_battery);
  text(u.hh_sinklabel, in.has_battery ? "CHG/EXP" : "EXPORT");
  text(u.hd_sinklabel, in.has_battery ? "CHG/EXP" : "EXPORT");
  show(u.hh_cap3, in.has_battery); show(u.hh_val3, in.has_battery);
  for (lv_obj_t *o : {u.hd_cap4, u.hd_val4, u.hd_cap5, u.hd_val5}) show(o, in.has_battery);

  struct tm tm; localtime_r(&in.now, &tm);
  int current = tm.tm_hour;
  if (h.hour < 0 || h.hour > current) h.hour = current;
  Energy hours[24], days[7];
  float hpos = 0, hneg = 0, dpos = 0, dneg = 0;
  for (int i = 0; i < 24; ++i) {
    if (in.valid && i <= current) hours[i] = history_energy(in.feed->hours[i]);
    if (!in.has_battery) { hours[i].bin = 0; hours[i].bout = 0; }
    hpos = fmaxf(hpos, ok(hist_source4(hours[i])) ? hist_source4(hours[i]) : 0);
    hneg = fmaxf(hneg, ok(hist_sink4(hours[i])) ? hist_sink4(hours[i]) : 0);
  }
  for (int i = 0; i < 7; ++i) {
    if (in.valid) days[i] = history_energy(in.feed->days[i + 1]);
    if (!in.has_battery) { days[i].bin = 0; days[i].bout = 0; }
    dpos = fmaxf(dpos, ok(hist_source4(days[i])) ? hist_source4(days[i]) : 0);
    dneg = fmaxf(dneg, ok(hist_sink4(days[i])) ? hist_sink4(days[i]) : 0);
  }
  hist_chart4(false, hours, 24, h.hour, hpos, hneg, in.has_battery);
  hist_chart4(true, days, 7, h.day, dpos, dneg, in.has_battery);
  textf(u.hh_range, "%02d:00 - %02d:00", h.hour, (h.hour + 1) % 24);
  Energy eh = hours[h.hour];
  float grid = ok(eh.imp) && ok(eh.exp) ? eh.imp - eh.exp : NAN;
  float battery = ok(eh.bin) && ok(eh.bout) ? eh.bin - eh.bout : NAN;
  hist_value4(u.hh_val0, eh.home(), HOME);
  hist_value4(u.hh_val1, eh.solar, SOLAR);
  hist_value4(u.hh_val2, grid, ok(grid) && grid < 0 ? EXPORT : IMPORT, true);
  if (in.has_battery) hist_value4(u.hh_val3, battery, BATT, true);

  time_t when = in.valid ? in.feed->dates[h.day + 1] : 0;
  if (!when) { struct tm date = tm; date.tm_mday -= 6 - h.day;
               date.tm_hour = 12; date.tm_isdst = -1; when = mktime(&date); }
  struct tm date; localtime_r(&when, &date);
  static const char *names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (h.day == 6) textf(u.hd_range, "TODAY, %s %d", names[date.tm_wday], date.tm_mday);
  else textf(u.hd_range, "%s %d", names[date.tm_wday], date.tm_mday);
  for (int i = 0; i < 7; ++i) {
    time_t t = in.valid ? in.feed->dates[i + 1] : 0;
    if (!t) { struct tm d = tm; d.tm_mday -= 6 - i; d.tm_hour = 12; d.tm_isdst = -1;
              t = mktime(&d); }
    struct tm d; localtime_r(&t, &d);
    lv_obj_t *labels[7] = {u.hd_day0, u.hd_day1, u.hd_day2, u.hd_day3,
                           u.hd_day4, u.hd_day5, u.hd_day6};
    text(labels[i], i == 6 ? "TODAY" : names[d.tm_wday]);
    color(labels[i], i == h.day ? TXT : TXT3, false);
  }
  Energy ed = days[h.day];
  hist_value4(u.hd_val0, ed.solar, SOLAR);
  hist_value4(u.hd_val1, ed.home(), HOME);
  hist_value4(u.hd_val2, ed.imp, IMPORT);
  hist_value4(u.hd_val3, ed.exp, EXPORT);
  if (in.has_battery) {
    hist_value4(u.hd_val4, ed.bin, BATT);
    hist_value4(u.hd_val5, ed.bout, BATT);
  }
}

inline void hist_step_4(char chart, int dir, time_t now) {
  Hist4 &h = hist4();
  if (chart == 'h') {
    struct tm tm; localtime_r(&now, &tm);
    h.hour = std::max(0, std::min(tm.tm_hour, (h.hour < 0 ? tm.tm_hour : h.hour) + dir));
  } else h.day = std::max(0, std::min(6, h.day + dir));
}
inline void hist_tap_4(char chart, time_t now) {
  lv_indev_t *ind = lv_indev_active();
  lv_obj_t *plot = chart == 'h' ? ui().hh_chart : ui().hd_chart;
  if (ind == nullptr || plot == nullptr) return;
  lv_point_t p; lv_indev_get_point(ind, &p);
  lv_area_t a; lv_obj_get_coords(plot, &a);
  int index = (p.x - a.x1) / (chart == 'h' ? 16 : 54);
  if (chart == 'h') { struct tm tm; localtime_r(&now, &tm);
                      hist4().hour = std::max(0, std::min(tm.tm_hour, index)); }
  else hist4().day = std::max(0, std::min(6, index));
}

inline void hist_mode(bool days) { hist4().days_mode = days; }
