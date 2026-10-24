// =============================================================================
//  On-panel Wi-Fi setup -- LVGL page (built at run time) and its logic.
//
//  The generated display YAML declares an empty container (`wf_root`) on page pg_wifi.
//  Everything inside is created here on first use, sized from that container, so the
//  4" (480x440) and 5" (688x440) panels share this file. The radio is reached only
//  through hooks() (wifi_link.h on the firmware, a fake in the host tests).
//
//  Screens: saved networks (+ "Add Wi-Fi" at the bottom, always) -> scan list ->
//  password / hidden-network name on an on-screen keyboard -> connecting -> result.
//  A network is stored only after the panel has joined it and the stored copy has been
//  read back. A failed attempt puts the previous networks back.
// =============================================================================
#pragma once
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "panel.h"
#include "wifi_setup.h"

namespace vp {
namespace wifi {

// ---------------------------------------------------------------- glyphs (Material Design Icons)
#define MDI_WIFI "\U000F05A9"
#define MDI_WIFI_OFF "\U000F05AA"
#define MDI_WIFI_1 "\U000F091F"
#define MDI_WIFI_2 "\U000F0922"
#define MDI_WIFI_3 "\U000F0925"
#define MDI_WIFI_4 "\U000F0928"
#define MDI_WIFI_ALERT "\U000F16B5"
#define MDI_LOCK "\U000F033E"
#define MDI_EYE "\U000F0208"
#define MDI_EYE_OFF "\U000F0209"
#define MDI_BACKSPACE "\U000F0B5C"
#define MDI_ENTER "\U000F0311"
#define MDI_SHIFT "\U000F0636"
#define MDI_TRASH "\U000F0A7A"
#define MDI_REFRESH "\U000F0450"
#define MDI_CHECK "\U000F012C"
#define MDI_ARROW_UP "\U000F005D"
#define MDI_ARROW_DOWN "\U000F0045"
#define MDI_PLUS_ "\U000F0415"
#define MDI_BACK "\U000F0141"
// Every glyph this page uses, for the generated font definitions.
#define MDI_WIFI_GLYPHS "F05A9 F05AA F091F F0922 F0925 F0928 F16B5 F033E F0208 F0209 F0B5C F0311 F0636 F0A7A F0450 F012C F005D F0045 F0415 F0141"

constexpr uint32_t GREEN = 0x25A35E, AMBER = 0xFFB020, RED = 0xF0524D;

struct Fonts {
  const lv_font_t *r12 = nullptr, *r14 = nullptr, *r16 = nullptr, *b16 = nullptr, *b20 = nullptr;
  const lv_font_t *i24 = nullptr, *i48 = nullptr;   // MDI icons
};
inline Fonts &fonts() { static Fonts f; return f; }

// What the page needs from the outside world.
struct Hooks {
  void (*scan)() = nullptr;                                  // start a scan (never blocks)
  void (*scan_results)(std::vector<Raw> &) = nullptr;        // what the radio has found so far
  void (*try_net)(const Net &) = nullptr;                    // join this one network (not stored)
  void (*use_saved)(const std::vector<Net> &) = nullptr;     // allow exactly these networks; empty = none
  bool (*connected)() = nullptr;                             // joined and has an address
  void (*cur_ssid)(char *, size_t) = nullptr;                // name of the network joined now
  void (*cur_ip)(char *, size_t) = nullptr;
  bool (*load)(Blob &) = nullptr;                            // persistence of the saved list
  bool (*store)(const Blob &) = nullptr;                     // true when written
  void (*reboot)() = nullptr;
  void (*open)() = nullptr;                                  // show the Wi-Fi page
  void (*back)() = nullptr;                                  // leave it for Settings > Network
  uint32_t (*now)() = nullptr;                               // milliseconds (tests use a fake clock)
};
inline Hooks &hooks() { static Hooks h; return h; }
inline uint32_t now_ms() { return hooks().now != nullptr ? hooks().now() : (uint32_t) lv_tick_get(); }

// ---------------------------------------------------------------- keyboard layers
enum Key { K_CHAR = 0, K_SHIFT = -1, K_BKSP = -2, K_SYM = -3, K_ABC = -4, K_MORE = -5, K_SPACE = -6, K_ENTER = -7 };
struct KeyDef { const char *t; int code; float w; };
constexpr int MAX_KEYS = 10;
struct Row { int n; KeyDef k[MAX_KEYS]; };
inline const Row *layer(int l) {
  static const Row L0[4] = {
    {10, {{"q", 0, 1}, {"w", 0, 1}, {"e", 0, 1}, {"r", 0, 1}, {"t", 0, 1}, {"y", 0, 1}, {"u", 0, 1}, {"i", 0, 1}, {"o", 0, 1}, {"p", 0, 1}}},
    {9, {{"a", 0, 1}, {"s", 0, 1}, {"d", 0, 1}, {"f", 0, 1}, {"g", 0, 1}, {"h", 0, 1}, {"j", 0, 1}, {"k", 0, 1}, {"l", 0, 1}}},
    {9, {{MDI_SHIFT, K_SHIFT, 1.5f}, {"z", 0, 1}, {"x", 0, 1}, {"c", 0, 1}, {"v", 0, 1}, {"b", 0, 1}, {"n", 0, 1}, {"m", 0, 1}, {MDI_BACKSPACE, K_BKSP, 1.5f}}},
    {5, {{"?123", K_SYM, 1.5f}, {",", 0, 1}, {" ", K_SPACE, 4}, {".", 0, 1}, {"", K_ENTER, 2.5f}}}};
  static const Row L1[4] = {
    {10, {{"Q", 0, 1}, {"W", 0, 1}, {"E", 0, 1}, {"R", 0, 1}, {"T", 0, 1}, {"Y", 0, 1}, {"U", 0, 1}, {"I", 0, 1}, {"O", 0, 1}, {"P", 0, 1}}},
    {9, {{"A", 0, 1}, {"S", 0, 1}, {"D", 0, 1}, {"F", 0, 1}, {"G", 0, 1}, {"H", 0, 1}, {"J", 0, 1}, {"K", 0, 1}, {"L", 0, 1}}},
    {9, {{MDI_SHIFT, K_SHIFT, 1.5f}, {"Z", 0, 1}, {"X", 0, 1}, {"C", 0, 1}, {"V", 0, 1}, {"B", 0, 1}, {"N", 0, 1}, {"M", 0, 1}, {MDI_BACKSPACE, K_BKSP, 1.5f}}},
    {5, {{"?123", K_SYM, 1.5f}, {",", 0, 1}, {" ", K_SPACE, 4}, {".", 0, 1}, {"", K_ENTER, 2.5f}}}};
  static const Row L2[4] = {
    {10, {{"1", 0, 1}, {"2", 0, 1}, {"3", 0, 1}, {"4", 0, 1}, {"5", 0, 1}, {"6", 0, 1}, {"7", 0, 1}, {"8", 0, 1}, {"9", 0, 1}, {"0", 0, 1}}},
    {10, {{"@", 0, 1}, {"#", 0, 1}, {"$", 0, 1}, {"%", 0, 1}, {"&", 0, 1}, {"-", 0, 1}, {"+", 0, 1}, {"(", 0, 1}, {")", 0, 1}, {"/", 0, 1}}},
    {9, {{"=\\<", K_MORE, 1.5f}, {"*", 0, 1}, {"\"", 0, 1}, {"'", 0, 1}, {":", 0, 1}, {";", 0, 1}, {"!", 0, 1}, {"?", 0, 1}, {MDI_BACKSPACE, K_BKSP, 1.5f}}},
    {5, {{"ABC", K_ABC, 1.5f}, {",", 0, 1}, {" ", K_SPACE, 4}, {".", 0, 1}, {"", K_ENTER, 2.5f}}}};
  static const Row L3[4] = {
    {10, {{"~", 0, 1}, {"`", 0, 1}, {"|", 0, 1}, {"{", 0, 1}, {"}", 0, 1}, {"[", 0, 1}, {"]", 0, 1}, {"<", 0, 1}, {">", 0, 1}, {"=", 0, 1}}},
    {7, {{"\\", 0, 1}, {"^", 0, 1}, {"_", 0, 1}, {"%", 0, 1}, {"#", 0, 1}, {"@", 0, 1}, {"$", 0, 1}}},
    {3, {{"?123", K_SYM, 1.5f}, {"&", 0, 1}, {MDI_BACKSPACE, K_BKSP, 1.5f}}},
    {5, {{"ABC", K_ABC, 1.5f}, {",", 0, 1}, {" ", K_SPACE, 4}, {".", 0, 1}, {"", K_ENTER, 2.5f}}}};
  switch (l) { case 1: return L1; case 2: return L2; case 3: return L3; default: return L0; }
}

// ---------------------------------------------------------------- state
enum View { V_LIST, V_SCAN, V_NAME, V_PASS, V_BUSY, V_RESULT, V_CONFIRM, V_COUNT };
enum Act {
  A_BACK = 1, A_ADD, A_RESCAN, A_OTHER, A_PREV, A_NEXT, A_EYE, A_RETRY, A_DONE, A_CANCEL, A_FORGET_OK,
  A_ROW = 20,      // +i  scan row
  A_DEL = 40,      // +i  forget button of saved row i
  A_KEY = 100,     // +r*10+k  keyboard key
};
constexpr int MAX_SCAN_ROWS = 7;
constexpr int KEY_H = 52;

struct State {
  bool built = false, booted = false;
  int W = 0, H = 0, scan_rows = 5;
  int view = V_LIST;
  Saved saved;
  // scan
  std::vector<Found> found;
  bool scanning = false;
  uint32_t scan_t0 = 0, scan_read = 0;
  int scan_page = 0;
  // entry
  std::string ssid_in, pass_in;
  bool hidden = false, secure = true, show_pw = false;
  int layer = 0;
  bool shift_once = false;
  // attempt / result
  Attempt att;
  bool res_ok = false, res_save_failed = false, was_connected_before = false;
  std::string res_title, res_sub;
  int forget_idx = -1;
  uint32_t reboot_at = 0;
  bool want_add = false;   // open the page straight on the network scan

  lv_obj_t *root = nullptr, *box[V_COUNT]{}, *title = nullptr, *back = nullptr, *back_ic = nullptr;
  // list
  lv_obj_t *l_row[MAX_SAVED]{}, *l_ic[MAX_SAVED]{}, *l_name[MAX_SAVED]{}, *l_sub[MAX_SAVED]{}, *l_del[MAX_SAVED]{},
           *l_del_ic[MAX_SAVED]{};
  lv_obj_t *l_empty_ic = nullptr, *l_empty_t = nullptr, *l_empty_s = nullptr, *l_note = nullptr, *l_add = nullptr,
           *l_add_l = nullptr, *l_add_ic = nullptr;
  // scan
  lv_obj_t *s_row[MAX_SCAN_ROWS]{}, *s_ic[MAX_SCAN_ROWS]{}, *s_name[MAX_SCAN_ROWS]{}, *s_tag[MAX_SCAN_ROWS]{},
           *s_lock[MAX_SCAN_ROWS]{};
  lv_obj_t *s_status = nullptr, *s_prev = nullptr, *s_next = nullptr, *s_pager = nullptr, *s_rescan = nullptr,
           *s_other = nullptr;
  // entry
  lv_obj_t *e_field = nullptr, *e_text = nullptr, *e_eye = nullptr, *e_eye_ic = nullptr, *e_hint = nullptr;
  lv_obj_t *k_btn[4][MAX_KEYS]{}, *k_lbl[4][MAX_KEYS]{};
  lv_obj_t *k_enter_l = nullptr, *k_enter_ic = nullptr;
  // busy / result / confirm
  lv_obj_t *b_ic = nullptr, *b_t = nullptr, *b_s = nullptr, *b_cancel = nullptr, *b_cancel_l = nullptr;
  lv_obj_t *r_ic = nullptr, *r_t = nullptr, *r_s = nullptr, *r_a = nullptr, *r_a_l = nullptr, *r_b = nullptr, *r_b_l = nullptr;
  lv_obj_t *c_t = nullptr, *c_s = nullptr, *c_a = nullptr, *c_a_l = nullptr, *c_b = nullptr, *c_b_l = nullptr;
};
inline State &S() { static State s; return s; }

inline int saved_count() { return (int) S().saved.n.size(); }
inline bool have_saved() { return !S().saved.n.empty(); }

// ---------------------------------------------------------------- widget helpers
inline void on_event(lv_event_t *e);
inline lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t col, int r = 10, bool opaque = true) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, opaque ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}
inline lv_obj_t *txt(lv_obj_t *p, int x, int y, int w, const lv_font_t *f, uint32_t col, const char *t = "",
                     lv_text_align_t al = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_pos(l, x, y);
  if (w > 0) lv_obj_set_width(l, w);
  if (f != nullptr) lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  lv_obj_set_style_text_align(l, al, 0);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_label_set_text(l, t);
  return l;
}
// Label centred vertically in a box.
inline lv_obj_t *txtc(lv_obj_t *p, int x, int y, int w, int h, const lv_font_t *f, uint32_t col, const char *t = "",
                      lv_text_align_t al = LV_TEXT_ALIGN_CENTER) {
  lv_obj_t *l = txt(p, x, y, w, f, col, t, al);
  int lh = f != nullptr ? lv_font_get_line_height(f) : 14;
  lv_obj_set_y(l, y + (h - lh) / 2);
  return l;
}
inline lv_obj_t *btn(lv_obj_t *p, int x, int y, int w, int h, uint32_t col, int r, int code) {
  lv_obj_t *o = box(p, x, y, w, h, col, r);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(o, lv_color_mix(lv_color_white(), lv_color_hex(col), 40), LV_STATE_PRESSED);
  lv_obj_add_event_cb(o, on_event, LV_EVENT_CLICKED, (void *) (intptr_t) code);
  return o;
}

inline void setfont_(lv_obj_t *o, const lv_font_t *f) {
  if (o != nullptr && f != nullptr && lv_obj_get_style_text_font(o, LV_PART_MAIN) != f) lv_obj_set_style_text_font(o, f, 0);
}

// ---------------------------------------------------------------- text entry
inline std::string &field() { State &u = S(); return u.view == V_NAME ? u.ssid_in : u.pass_in; }
inline size_t field_max() { return S().view == V_NAME ? SSID_MAX : PASS_MAX; }
inline bool entry_ok() {
  State &u = S();
  if (u.view == V_NAME) return valid_ssid(u.ssid_in);
  if (u.view == V_PASS) return u.secure ? (u.pass_in.size() >= PASS_MIN && u.pass_in.size() <= PASS_MAX) : valid_pass(u.pass_in);
  return false;
}
inline void press_char(const char *t) {
  State &u = S();
  std::string &f = field();
  if (f.size() + strlen(t) > field_max()) return;
  f += t;
  if (u.shift_once) { u.shift_once = false; u.layer = 0; }
}

// ---------------------------------------------------------------- actions
inline void set_view(int v) { S().view = v; }

inline void start_scan() {
  State &u = S();
  u.found.clear();
  u.scan_page = 0;
  u.scanning = true;
  u.scan_t0 = now_ms();
  u.scan_read = u.scan_t0;
  if (hooks().scan != nullptr) hooks().scan();
}

inline void read_scan() {
  State &u = S();
  if (hooks().scan_results == nullptr) return;
  std::vector<Raw> raw;
  hooks().scan_results(raw);
  u.found = clean_scan(raw);
  if (u.att.active)
    for (const Found &f : u.found)
      if (f.ssid == u.att.ssid) u.att.seen = true;
  int pages = ((int) u.found.size() + u.scan_rows - 1) / u.scan_rows;
  if (u.scan_page >= pages) u.scan_page = pages > 0 ? pages - 1 : 0;
}

inline void restore_radio() {
  if (hooks().use_saved != nullptr) hooks().use_saved(S().saved.n);
}

inline void begin_attempt(const std::string &ssid, const std::string &pass, bool hidden) {
  State &u = S();
  u.att = Attempt();
  u.att.active = true;
  u.att.ssid = ssid;
  u.att.pass = pass;
  u.att.hidden = hidden;
  u.att.t0 = now_ms();
  for (const Found &f : u.found)
    if (f.ssid == ssid) u.att.seen = true;
  u.was_connected_before = hooks().connected != nullptr && hooks().connected();
  Net n;
  n.ssid = ssid; n.pass = pass; n.hidden = hidden;
  if (hooks().try_net != nullptr) hooks().try_net(n);
  u.view = V_BUSY;
}

inline bool persist() {
  State &u = S();
  if (hooks().store == nullptr || hooks().load == nullptr) return false;
  Blob want = pack(u.saved);
  if (!hooks().store(want)) return false;
  Blob back{};
  return hooks().load(back) && same(want, back);
}

inline void finish_ok() {
  State &u = S();
  Saved before = u.saved;
  u.saved.add(u.att.ssid, u.att.pass, u.att.hidden);
  bool stored = persist();
  if (!stored) u.saved = before;   // never claim a network is stored when it is not
  char ip[24] = "";
  if (hooks().cur_ip != nullptr) hooks().cur_ip(ip, sizeof ip);
  u.res_ok = true;
  u.res_save_failed = !stored;
  u.res_title = "Connected";
  u.res_sub = "Joined " + u.att.ssid + (ip[0] ? std::string("  |  ") + ip : std::string(""));
  if (!stored) u.res_sub += "\nThe network could not be saved and will be forgotten after a restart.";
  u.att = Attempt();
  u.view = V_RESULT;
}

inline void finish_fail(Res r) {
  State &u = S();
  u.res_ok = false;
  u.res_save_failed = false;
  if (r == Res::NOT_FOUND) {
    u.res_title = "Network not found";
    u.res_sub = "\"" + u.att.ssid + "\" was not seen. Move closer to the router, or check the name.";
  } else {
    u.res_title = "Could not connect";
    u.res_sub = u.att.hidden ? "Could not join \"" + u.att.ssid + "\". Check the network name and the password."
                             : "\"" + u.att.ssid + "\" did not accept the panel. Check the password and try again.";
  }
  u.saved.mark(u.att.ssid, r == Res::NOT_FOUND ? L_NOT_FOUND : L_FAILED);
  u.att = Attempt();
  restore_radio();
  u.view = V_RESULT;
}

inline void tick() {
  State &u = S();
  uint32_t t = now_ms();
  if (u.scanning) {
    // Results arrive a few seconds after the scan starts; keep reading until they settle.
    if (t - u.scan_t0 >= SCAN_SETTLE_MS) { read_scan(); u.scanning = false; }
    else if (t - u.scan_read >= 700) { u.scan_read = t; read_scan(); }
  }
  if (u.att.active) {
    char cur[40] = "";
    if (hooks().cur_ssid != nullptr) hooks().cur_ssid(cur, sizeof cur);
    bool up = hooks().connected != nullptr && hooks().connected();
    // About to give up: look at the radio's own scan list once more. A name that is not on air is
    // "not found"; one that is on air but would not accept the panel has a wrong password.
    if (!up && !u.att.hidden && t - u.att.t0 >= CONNECT_TIMEOUT_MS && hooks().scan_results != nullptr) {
      std::vector<Raw> raw;
      hooks().scan_results(raw);
      std::vector<Found> f = clean_scan(raw);
      if (!f.empty()) {
        u.att.seen = false;
        for (const Found &x : f)
          if (x.ssid == u.att.ssid) u.att.seen = true;
      }
    }
    Res r = judge(u.att, up, cur, t);
    if (r == Res::OK) finish_ok();
    else if (r == Res::NOT_FOUND || r == Res::FAILED) finish_fail(r);
  }
  if (u.reboot_at != 0 && (int32_t) (t - u.reboot_at) >= 0) {
    u.reboot_at = 0;
    if (hooks().reboot != nullptr) hooks().reboot();
  }
}

inline void act(int code) {
  State &u = S();
  switch (code) {
    case A_BACK:
      switch (u.view) {
        case V_LIST: if (hooks().back != nullptr) hooks().back(); break;
        case V_SCAN: u.view = V_LIST; break;
        case V_NAME: u.view = V_SCAN; break;
        case V_PASS: u.view = u.hidden ? V_NAME : V_SCAN; break;
        case V_RESULT: case V_CONFIRM: u.view = V_LIST; break;
        default: break;   // V_BUSY: only Cancel leaves
      }
      return;
    case A_ADD: u.view = V_SCAN; start_scan(); return;
    case A_RESCAN: start_scan(); return;
    case A_OTHER:
      u.hidden = true; u.secure = false; u.ssid_in.clear(); u.pass_in.clear(); u.show_pw = false; u.layer = 0;
      u.view = V_NAME;
      return;
    case A_PREV: if (u.scan_page > 0) u.scan_page--; return;
    case A_NEXT: if ((u.scan_page + 1) * u.scan_rows < (int) u.found.size()) u.scan_page++; return;
    case A_EYE: u.show_pw = !u.show_pw; return;
    case A_RETRY: u.view = V_PASS; return;
    case A_DONE: u.view = V_LIST; return;
    case A_CANCEL:
      if (u.att.active) { u.att = Attempt(); restore_radio(); }
      u.view = V_LIST;
      return;
    case A_FORGET_OK: {
      if (u.forget_idx < 0 || u.forget_idx >= (int) u.saved.n.size()) { u.view = V_LIST; return; }
      std::string name = u.saved.n[u.forget_idx].ssid;
      char cur[40] = "";
      if (hooks().cur_ssid != nullptr) hooks().cur_ssid(cur, sizeof cur);
      bool joined = hooks().connected != nullptr && hooks().connected() && name == cur;
      Saved before = u.saved;
      u.saved.forget(u.forget_idx);
      if (!persist()) u.saved = before;
      else if (joined) u.reboot_at = now_ms() + 1500;   // the radio still holds the old list: restart to drop it
      u.forget_idx = -1;
      u.view = V_LIST;
      return;
    }
    default: break;
  }
  if (code >= A_ROW && code < A_ROW + MAX_SCAN_ROWS) {
    int i = u.scan_page * u.scan_rows + (code - A_ROW);
    if (i < 0 || i >= (int) u.found.size()) return;
    const Found &f = u.found[i];
    u.hidden = false; u.secure = f.secure; u.ssid_in = f.ssid; u.pass_in.clear(); u.show_pw = false; u.layer = 0;
    u.shift_once = false;
    if (!f.secure) begin_attempt(f.ssid, "", false);
    else u.view = V_PASS;
    return;
  }
  if (code >= A_DEL && code < A_DEL + MAX_SAVED) {
    int i = code - A_DEL;
    if (i < (int) u.saved.n.size()) { u.forget_idx = i; u.view = V_CONFIRM; }
    return;
  }
  if (code >= A_KEY && code < A_KEY + 40) {
    int r = (code - A_KEY) / 10, k = (code - A_KEY) % 10;
    const Row &row = layer(u.layer)[r];
    if (k >= row.n) return;
    const KeyDef &d = row.k[k];
    switch (d.code) {
      case K_CHAR: press_char(d.t); break;
      case K_SPACE: press_char(" "); break;
      case K_BKSP: { std::string &f = field(); while (!f.empty() && (f.back() & 0xC0) == 0x80) f.pop_back(); if (!f.empty()) f.pop_back(); break; }
      case K_SHIFT: u.layer = u.layer == 1 ? 0 : 1; u.shift_once = u.layer == 1; break;
      case K_SYM: u.layer = 2; u.shift_once = false; break;
      case K_ABC: u.layer = 0; u.shift_once = false; break;
      case K_MORE: u.layer = 3; break;
      case K_ENTER:
        if (!entry_ok()) break;
        if (u.view == V_NAME) { u.pass_in.clear(); u.show_pw = false; u.layer = 0; u.view = V_PASS; }
        else begin_attempt(u.ssid_in, u.pass_in, u.hidden);
        break;
      default: break;
    }
  }
}

inline void refresh();
inline void on_event(lv_event_t *e) {
  act((int) (intptr_t) lv_event_get_user_data(e));
  refresh();
}

// ---------------------------------------------------------------- build
inline lv_obj_t *icon_btn(lv_obj_t *p, int x, int y, int w, int h, uint32_t col, int code, const lv_font_t *f,
                          const char *glyph, uint32_t gcol, lv_obj_t **glyph_out = nullptr) {
  lv_obj_t *b = btn(p, x, y, w, h, col, 8, code);
  lv_obj_t *g = txtc(b, 0, 0, w, h, f, gcol, glyph);
  if (glyph_out != nullptr) *glyph_out = g;
  return b;
}

inline void build_list(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_LIST];
  for (int i = 0; i < MAX_SAVED; i++) {
    int y = 56 + i * 72;
    u.l_row[i] = box(v, 8, y, W - 16, 64, CARD, 10);
    u.l_ic[i] = txtc(u.l_row[i], 8, 0, 40, 64, F.i24, TXT2, MDI_WIFI);
    u.l_name[i] = txt(u.l_row[i], 56, 10, W - 16 - 56 - 64, F.b16, TXT, "");
    u.l_sub[i] = txt(u.l_row[i], 56, 36, W - 16 - 56 - 64, F.r12, TXT2, "");
    u.l_del[i] = icon_btn(u.l_row[i], W - 16 - 56, 8, 48, 48, RAISED, A_DEL + i, F.i24, MDI_TRASH, TXT2, &u.l_del_ic[i]);
  }
  u.l_empty_ic = txt(v, 0, 70, W, F.i48, TXT3, MDI_WIFI_OFF, LV_TEXT_ALIGN_CENTER);
  u.l_empty_t = txt(v, 16, 140, W - 32, F.b20, TXT, "No Wi-Fi network saved", LV_TEXT_ALIGN_CENTER);
  u.l_empty_s = txt(v, 24, 176, W - 48, F.r14, TXT2, "Add your home network so this panel can receive data from Home Assistant.", LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(u.l_empty_s, LV_LABEL_LONG_WRAP);
  u.l_note = txt(v, 12, H - 92, W - 24, F.r12, TXT3, "Adding a fourth network replaces the oldest one.", LV_TEXT_ALIGN_CENTER);
  u.l_add = btn(v, 8, H - 64, W - 16, 56, ACCENT, 10, A_ADD);
  u.l_add_ic = txtc(u.l_add, 0, 0, 52, 56, F.i24, TXT, MDI_PLUS_);
  u.l_add_l = txtc(u.l_add, 44, 0, W - 16 - 44, 56, F.b16, TXT, "ADD WI-FI", LV_TEXT_ALIGN_LEFT);
  // centre icon + text as a group
  lv_obj_set_x(u.l_add_ic, (W - 16) / 2 - 76);
  lv_obj_set_x(u.l_add_l, (W - 16) / 2 - 76 + 40);
  lv_obj_set_width(u.l_add_l, 160);
}

inline void build_scan(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_SCAN];
  for (int i = 0; i < MAX_SCAN_ROWS; i++) {
    int y = 56 + i * 56;
    u.s_row[i] = btn(v, 8, y, W - 16, 52, CARD, 10, A_ROW + i);
    u.s_ic[i] = txtc(u.s_row[i], 8, 0, 40, 52, F.i24, TXT, MDI_WIFI_3);
    u.s_name[i] = txtc(u.s_row[i], 56, 0, W - 16 - 56 - 100, 52, F.b16, TXT, "", LV_TEXT_ALIGN_LEFT);
    u.s_tag[i] = txtc(u.s_row[i], W - 16 - 100 - 36, 0, 100, 52, F.r12, TXT3, "", LV_TEXT_ALIGN_RIGHT);
    u.s_lock[i] = txtc(u.s_row[i], W - 16 - 40, 0, 32, 52, F.i24, TXT2, MDI_LOCK);
  }
  u.s_status = txt(v, 16, 120, W - 32, F.r14, TXT2, "", LV_TEXT_ALIGN_CENTER);
  int bw = (W - 24) / 2;
  u.s_rescan = btn(v, 8, H - 64, bw, 56, RAISED, 10, A_RESCAN);
  txtc(u.s_rescan, 0, 0, bw, 56, F.b16, TXT, "RESCAN");
  u.s_other = btn(v, 16 + bw, H - 64, bw, 56, RAISED, 10, A_OTHER);
  txtc(u.s_other, 0, 0, bw, 56, F.b16, TXT, "OTHER NETWORK");
  u.s_prev = icon_btn(v, W - 8 - 44 * 2 - 6, 4, 44, 44, RAISED, A_PREV, F.i24, MDI_ARROW_UP, TXT);
  u.s_next = icon_btn(v, W - 8 - 44, 4, 44, 44, RAISED, A_NEXT, F.i24, MDI_ARROW_DOWN, TXT);
  u.s_pager = txt(v, W - 8 - 44 * 2 - 6 - 70, 16, 64, F.r12, TXT3, "", LV_TEXT_ALIGN_RIGHT);
}

inline void build_entry(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_PASS];   // shared by V_NAME and V_PASS
  u.e_field = box(v, 8, 56, W - 16, 56, CARD, 10);
  u.e_text = txtc(u.e_field, 14, 0, W - 16 - 28 - 56, 56, F.b20, TXT, "", LV_TEXT_ALIGN_LEFT);
  u.e_eye = icon_btn(v, W - 8 - 52, 58, 52, 52, RAISED, A_EYE, F.i24, MDI_EYE, TXT2, &u.e_eye_ic);
  u.e_hint = txt(v, 12, 118, W - 24, F.r12, TXT3, "");
  // keyboard: buttons are created here, positioned per layer in place_keys()
  for (int r = 0; r < 4; r++)
    for (int k = 0; k < MAX_KEYS; k++) {
      lv_obj_t *b = btn(v, 0, 0, 40, KEY_H, RAISED, 8, A_KEY + r * 10 + k);
      u.k_btn[r][k] = b;
      u.k_lbl[r][k] = txtc(b, 0, 0, 40, KEY_H, F.b20, TXT, "");
    }
  (void) H;
}

// Lay the keys of the current layer out: rows centred, widths by weight.
inline void place_keys(State &u) {
  const int gap = 4;
  float unit = (u.W - 16 - 9 * gap) / 10.0f;
  int y0 = u.H - 4 * (KEY_H + gap) - 4 + gap;
  for (int r = 0; r < 4; r++) {
    const Row &row = layer(u.layer)[r];
    int w[MAX_KEYS];
    int total = (row.n - 1) * gap;
    for (int k = 0; k < row.n; k++) {
      w[k] = (int) (row.k[k].w * unit + (row.k[k].w - 1) * gap);   // wide keys absorb the gaps they span
      total += w[k];
    }
    int x = 8 + ((u.W - 16) - total) / 2;
    for (int k = 0; k < row.n; k++) {
      lv_obj_set_pos(u.k_btn[r][k], x, y0 + r * (KEY_H + gap));
      lv_obj_set_size(u.k_btn[r][k], w[k], KEY_H);
      lv_obj_set_width(u.k_lbl[r][k], w[k]);
      x += w[k] + gap;
    }
  }
}

inline void build_busy(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_BUSY];
  u.b_ic = txt(v, 0, 80, W, F.i48, AMBER, MDI_WIFI, LV_TEXT_ALIGN_CENTER);
  u.b_t = txt(v, 16, 160, W - 32, F.b20, TXT, "", LV_TEXT_ALIGN_CENTER);
  u.b_s = txt(v, 24, 196, W - 48, F.r14, TXT2, "", LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(u.b_s, LV_LABEL_LONG_WRAP);
  u.b_cancel = btn(v, 8, H - 64, W - 16, 56, RAISED, 10, A_CANCEL);
  u.b_cancel_l = txtc(u.b_cancel, 0, 0, W - 16, 56, F.b16, TXT, "CANCEL");
}

inline void build_result(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_RESULT];
  u.r_ic = txt(v, 0, 80, W, F.i48, GREEN, MDI_CHECK, LV_TEXT_ALIGN_CENTER);
  u.r_t = txt(v, 16, 160, W - 32, F.b20, TXT, "", LV_TEXT_ALIGN_CENTER);
  u.r_s = txt(v, 24, 196, W - 48, F.r14, TXT2, "", LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(u.r_s, LV_LABEL_LONG_WRAP);
  int bw = (W - 24) / 2;
  u.r_a = btn(v, 8, H - 64, bw, 56, ACCENT, 10, A_RETRY);
  u.r_a_l = txtc(u.r_a, 0, 0, bw, 56, F.b16, TXT, "TRY AGAIN");
  u.r_b = btn(v, 16 + bw, H - 64, bw, 56, RAISED, 10, A_DONE);
  u.r_b_l = txtc(u.r_b, 0, 0, bw, 56, F.b16, TXT, "BACK");
}

inline void build_confirm(State &u, const Fonts &F) {
  int W = u.W, H = u.H;
  lv_obj_t *v = u.box[V_CONFIRM];
  u.c_t = txt(v, 16, 110, W - 32, F.b20, TXT, "", LV_TEXT_ALIGN_CENTER);
  u.c_s = txt(v, 24, 150, W - 48, F.r14, TXT2, "", LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(u.c_s, LV_LABEL_LONG_WRAP);
  int bw = (W - 24) / 2;
  u.c_a = btn(v, 8, H - 64, bw, 56, RAISED, 10, A_DONE);
  u.c_a_l = txtc(u.c_a, 0, 0, bw, 56, F.b16, TXT, "CANCEL");
  u.c_b = btn(v, 16 + bw, H - 64, bw, 56, RED, 10, A_FORGET_OK);
  u.c_b_l = txtc(u.c_b, 0, 0, bw, 56, F.b16, TXT, "FORGET");
}

inline void build(lv_obj_t *root) {
  State &u = S();
  const Fonts &F = fonts();
  u.root = root;
  lv_obj_update_layout(root);   // coordinates are stale until a layout pass
  u.W = lv_obj_get_width(root);
  u.H = lv_obj_get_height(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  u.scan_rows = (u.H - 56 - 72) / 56;
  if (u.scan_rows > MAX_SCAN_ROWS) u.scan_rows = MAX_SCAN_ROWS;
  if (u.scan_rows < 3) u.scan_rows = 3;
  // header: back button + title, shared by every view
  u.back = btn(root, 8, 4, 44, 44, RAISED, 8, A_BACK);
  u.back_ic = txtc(u.back, 0, 0, 44, 44, F.i24, TXT, MDI_BACK);
  u.title = txtc(root, 64, 4, u.W - 64 - 8 - 120, 44, F.b16, TXT, "Wi-Fi networks", LV_TEXT_ALIGN_LEFT);
  for (int i = 0; i < V_COUNT; i++) {
    if (i == V_NAME) continue;   // V_NAME shares V_PASS's widgets
    u.box[i] = box(root, 0, 0, u.W, u.H, BG, 0, false);
  }
  build_list(u, F);
  build_scan(u, F);
  build_entry(u, F);
  build_busy(u, F);
  build_result(u, F);
  build_confirm(u, F);
  u.built = true;
  // The header must stay on top of the view boxes (they are transparent but cover the page).
  lv_obj_move_foreground(u.back);
  lv_obj_move_foreground(u.title);
}

// ---------------------------------------------------------------- render
inline const char *signal_icon(int rssi) {
  switch (bars(rssi)) { case 4: return MDI_WIFI_4; case 3: return MDI_WIFI_3; case 2: return MDI_WIFI_2; default: return MDI_WIFI_1; }
}
inline const char *quality(int rssi) { return rssi >= -60 ? "Good signal" : rssi >= -72 ? "Fair signal" : "Weak signal"; }

inline std::string masked(size_t n) {
  std::string s;
  for (size_t i = 0; i < n; i++) s += "\xE2\x80\xA2";   // U+2022
  return s;
}

inline void render_list(State &u) {
  char cur[40] = "";
  if (hooks().cur_ssid != nullptr) hooks().cur_ssid(cur, sizeof cur);
  bool up = hooks().connected != nullptr && hooks().connected();
  int n = (int) u.saved.n.size();
  for (int i = 0; i < MAX_SAVED; i++) {
    show(u.l_row[i], i < n);
    if (i >= n) continue;
    const Net &x = u.saved.n[i];
    bool joined = up && x.ssid == cur;
    text(u.l_name[i], x.ssid.c_str());
    char sub[64];
    if (joined) snprintf(sub, sizeof sub, "Connected");
    else if (x.last == L_FAILED) snprintf(sub, sizeof sub, "Could not connect last time");
    else if (x.last == L_NOT_FOUND) snprintf(sub, sizeof sub, "Not found last time");
    else snprintf(sub, sizeof sub, "Saved%s", x.hidden ? " (hidden network)" : "");
    text(u.l_sub[i], sub);
    color(u.l_sub[i], joined ? GREEN : (x.last == L_FAILED || x.last == L_NOT_FOUND) ? AMBER : TXT2, false);
    text(u.l_ic[i], MDI_WIFI);
    color(u.l_ic[i], joined ? GREEN : TXT2, false);
  }
  show(u.l_empty_ic, n == 0);
  show(u.l_empty_t, n == 0);
  show(u.l_empty_s, n == 0);
  show(u.l_note, n >= MAX_SAVED);
  text(u.l_add_l, "ADD WI-FI");
}

inline void render_scan(State &u) {
  int total = (int) u.found.size();
  int pages = (total + u.scan_rows - 1) / u.scan_rows;
  for (int i = 0; i < MAX_SCAN_ROWS; i++) {
    int k = u.scan_page * u.scan_rows + i;
    bool on = i < u.scan_rows && k < total;
    show(u.s_row[i], on);
    if (!on) continue;
    const Found &f = u.found[k];
    text(u.s_ic[i], signal_icon(f.rssi));
    text(u.s_name[i], f.ssid.c_str());
    text(u.s_tag[i], u.saved.find(f.ssid) >= 0 ? "SAVED" : "");
    show(u.s_lock[i], f.secure);
  }
  if (total == 0) text(u.s_status, u.scanning ? "Looking for networks..." : "No networks found. Move closer to the router and rescan.");
  else text(u.s_status, "");
  show(u.s_status, total == 0);
  if (total > 0 && u.scanning) text(u.title, "Choose network  (scanning...)"); else text(u.title, "Choose network");
  show(u.s_prev, pages > 1);
  show(u.s_next, pages > 1);
  show(u.s_pager, pages > 1);
  if (pages > 1) textf(u.s_pager, "%d / %d", u.scan_page + 1, pages);
  bg(u.s_prev, u.scan_page > 0 ? RAISED : CARD);
  bg(u.s_next, u.scan_page + 1 < pages ? RAISED : CARD);
}

inline void render_entry(State &u) {
  bool name = u.view == V_NAME;
  std::string shown = name ? u.ssid_in : (u.show_pw ? u.pass_in : masked(u.pass_in.size()));
  shown += "|";
  text(u.e_text, shown.c_str());
  show(u.e_eye, !name);
  text(u.e_eye_ic, u.show_pw ? MDI_EYE_OFF : MDI_EYE);
  lv_obj_set_width(u.e_text, name ? u.W - 16 - 28 : u.W - 16 - 28 - 56);
  char b[96];
  if (name) {
    snprintf(b, sizeof b, "Network name, up to %d characters. Case matters.", (int) SSID_MAX);
    text(u.title, "Other network");
  } else {
    snprintf(b, sizeof b, u.secure ? "Password for %s  (%d to %d characters)" : "Password for %s  (leave empty for an open network)",
             u.ssid_in.c_str(), (int) PASS_MIN, (int) PASS_MAX);
    text(u.title, "Password");
  }
  text(u.e_hint, b);
  bool ok = entry_ok();
  place_keys(u);
  for (int r = 0; r < 4; r++) {
    const Row &row = layer(u.layer)[r];
    for (int k = 0; k < MAX_KEYS; k++) {
      bool on = k < row.n;
      show(u.k_btn[r][k], on);
      if (!on) continue;
      const KeyDef &d = row.k[k];
      lv_obj_t *l = u.k_lbl[r][k];
      bool icon = d.code == K_SHIFT || d.code == K_BKSP || d.code == K_ENTER;
      const Fonts &F = fonts();
      setfont_(l, icon ? F.i24 : F.b20);
      if (d.code == K_ENTER) {
        text(l, name ? "NEXT" : "CONNECT");
        setfont_(l, F.b16);
        color(l, ok ? TXT : TXT3, false);
        bg(u.k_btn[r][k], ok ? ACCENT : RAISED);
      } else {
        text(l, d.t);
        bool active = (d.code == K_SHIFT && u.layer == 1);
        color(l, d.code == K_SPACE ? TXT3 : TXT, false);
        bg(u.k_btn[r][k], active ? ACCENT : (d.code == K_CHAR || d.code == K_SPACE) ? RAISED : CARD);
      }
    }
  }
}

inline void render_busy(State &u) {
  text(u.title, "Connecting");
  char b[96];
  snprintf(b, sizeof b, "Connecting to %s", u.att.ssid.c_str());
  text(u.b_t, b);
  uint32_t el = (now_ms() - u.att.t0) / 1000;
  snprintf(b, sizeof b, "%us of %us. If the password is wrong this ends with an error.", (unsigned) el, (unsigned) (CONNECT_TIMEOUT_MS / 1000));
  text(u.b_s, b);
}

inline void render_result(State &u) {
  text(u.title, u.res_ok ? "Connected" : "Wi-Fi");
  text(u.r_ic, u.res_ok ? MDI_CHECK : MDI_WIFI_ALERT);
  color(u.r_ic, u.res_ok ? GREEN : RED, false);
  text(u.r_t, u.res_title.c_str());
  text(u.r_s, u.res_sub.c_str());
  show(u.r_a, !u.res_ok);
  if (u.res_ok) {   // a single wide button
    lv_obj_set_x(u.r_b, 8); lv_obj_set_width(u.r_b, u.W - 16); lv_obj_set_width(u.r_b_l, u.W - 16);
    text(u.r_b_l, "DONE");
    bg(u.r_b, ACCENT);
  } else {
    int bw = (u.W - 24) / 2;
    lv_obj_set_x(u.r_b, 16 + bw); lv_obj_set_width(u.r_b, bw); lv_obj_set_width(u.r_b_l, bw);
    text(u.r_b_l, "BACK");
    bg(u.r_b, RAISED);
  }
}

inline void render_confirm(State &u) {
  text(u.title, "Forget network");
  if (u.forget_idx < 0 || u.forget_idx >= (int) u.saved.n.size()) { text(u.c_t, ""); text(u.c_s, ""); return; }
  const Net &x = u.saved.n[u.forget_idx];
  char cur[40] = "";
  if (hooks().cur_ssid != nullptr) hooks().cur_ssid(cur, sizeof cur);
  bool joined = hooks().connected != nullptr && hooks().connected() && x.ssid == cur;
  char b[96];
  snprintf(b, sizeof b, "Forget %s?", x.ssid.c_str());
  text(u.c_t, b);
  text(u.c_s, joined ? "The panel is connected to this network now. It will restart and join another saved network, if there is one."
                     : "The panel will no longer join this network.");
}

inline void render(lv_obj_t *root) {
  State &u = S();
  if (root == nullptr) return;
  if (!u.built) build(root);
  tick();
  bool entry = u.view == V_NAME || u.view == V_PASS;
  for (int i = 0; i < V_COUNT; i++) {
    if (u.box[i] == nullptr) continue;
    bool on = i == (entry ? V_PASS : u.view);
    show(u.box[i], on);
  }
  switch (u.view) {
    case V_LIST: text(u.title, "Wi-Fi networks"); render_list(u); break;
    case V_SCAN: render_scan(u); break;
    case V_NAME: case V_PASS: render_entry(u); break;
    case V_BUSY: render_busy(u); break;
    case V_RESULT: render_result(u); break;
    case V_CONFIRM: render_confirm(u); break;
    default: break;
  }
  show(u.back, u.view != V_BUSY);
  lv_obj_set_x(u.title, u.view == V_BUSY ? 16 : 64);
}
inline void refresh() { if (S().built) render(S().root); }

// The next time the page opens, start on the network scan instead of the list.
inline void request_add() { S().want_add = true; }
// Open the page on the saved-network list.
inline void request_list() { S().want_add = false; }

// Page opened: start on the saved-network list (or the scan when request_add() was called),
// unless a connection attempt is still running.
inline void entered() {
  State &u = S();
  bool add = u.want_add;
  u.want_add = false;
  u.show_pw = false;
  if (u.att.active) { u.view = V_BUSY; return; }
  u.view = V_LIST;
  if (add) { u.view = V_SCAN; start_scan(); }
}

// Called once after the radio is up: restore the saved networks.
inline void boot() {
  State &u = S();
  if (u.booted) return;
  u.booted = true;
  Blob b{};
  if (hooks().load != nullptr && hooks().load(b)) {
    if (!unpack(b, u.saved)) u.saved.n.clear();
  }
  if (!u.saved.n.empty() && hooks().use_saved != nullptr) hooks().use_saved(u.saved.n);
}

// Wi-Fi symbol in the status bar: a tap opens this page.
inline void on_icon(lv_event_t *) {
  if (!have_saved()) request_add();   // nothing saved yet: go straight to choosing a network
  if (hooks().open != nullptr) hooks().open();
}
inline void init_icon(lv_obj_t *icon) {
  if (icon == nullptr) return;
  lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(icon, 12);
  lv_obj_add_event_cb(icon, on_icon, LV_EVENT_CLICKED, nullptr);
}

}  // namespace wifi
}  // namespace vp
