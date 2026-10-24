// Page codes and the fixed bottom-bar / side-rail slots.
//
// Slot 0..3 = NOW, TODAY, HISTORY, FORECAST. Codes 4..7 are the Settings pages
// (NETWORK, MPPT, DISPLAY, DEVICE) and 8 is the on-panel Wi-Fi setup page.
#pragma once

namespace vp {

constexpr int PG_NOW = 0, PG_TODAY = 1, PG_HIST = 2, PG_FC = 3, PG_WIFI = 8;

// Main pages show the bottom bar / side rail; every other page is a settings-style page.
inline bool is_main_page(int p) { return p >= 0 && p < 4; }

inline const char *page_name(int p) {
  switch (p) {
    case PG_NOW: return "NOW";
    case PG_TODAY: return "TODAY";
    case PG_HIST: return "HISTORY";
    case PG_FC: return "FORECAST";
    default: return "";
  }
}

// Page code shown in bar slot 0..3.
inline int nav_page(int slot) { return slot < 0 ? PG_NOW : (slot > 3 ? PG_FC : slot); }

}  // namespace vp
