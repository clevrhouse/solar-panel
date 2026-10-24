#!/usr/bin/env python3
"""Build the JC8048W550 800x480 UI from the shared Victron widget tree.

The rendering/data IDs are intentionally identical to the 4-inch source. The
content area grows horizontally and the four page buttons move to a side rail.
"""
from pathlib import Path
import yaml
import gen_display as base

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "victron_5in" / "display-guition-jc8048w550.yaml"
SCALE = 688 / 480
OFFSET = 112


def scaled(value):
    return round(value * SCALE)


def resize_tree(widget, root=False):
    """Resize horizontal geometry, leaving LVGL's font and event settings intact."""
    for kind, cfg in widget.items():
        if not isinstance(cfg, dict):
            continue
        right_aligned = cfg.get("align") in ("TOP_RIGHT", "RIGHT_MID", "BOTTOM_RIGHT")
        if isinstance(cfg.get("x"), int) and cfg.get("align") in ("CENTER", "TOP_MID", "BOTTOM_MID") and root:
            # x is an offset from the parent's centre. The source is relative to a
            # 480 px wide screen; convert via the absolute centre point so the
            # widget lands on the same content (800 px wide here).
            cfg["x"] = scaled(cfg["x"] + 240) + OFFSET - 400
        elif isinstance(cfg.get("x"), int):
            cfg["x"] = scaled(cfg["x"]) + (OFFSET if root and not right_aligned else 0)
        elif root and cfg.get("align") is None:
            cfg["x"] = OFFSET
        if isinstance(cfg.get("width"), int):
            cfg["width"] = scaled(cfg["width"])
        for child in cfg.get("widgets", []):
            resize_tree(child)


def nav_rail():
    items = []
    for i, (name, _page) in enumerate((("NOW", "pg_now"), ("TODAY", "pg_today"),
                                       ("HISTORY", "pg_hist"), ("FORECAST", "pg_fc"))):
        items.append({"obj": {"id": f"nav_bg{i}", "x": 6, "y": 66 + 94 * i,
                              "width": 100, "height": 76, "bg_color": base.RAISED,
                              "bg_opa": "COVER", "radius": 10, "border_width": 0,
                              "pad_all": 0, "scrollable": False, "clickable": True,
                              "on_click": [{"script.execute": {"id": "nav_go", "slot": i}}], "widgets": [
                                  {"label": {"id": f"nav_lbl{i}", "text": name,
                                             "text_font": "f7_12", "text_color": base.TXT2,
                                             "align": "CENTER"}},
                                  {"obj": {"id": f"nav_ul{i}", "align": "LEFT_MID",
                                           "width": 3, "height": 36, "bg_color": base.ACCENT,
                                           "bg_opa": "COVER", "radius": 2,
                                           "border_width": 0, "clickable": False,
                                           "hidden": True}}
                              ]}})
    return {"obj": {"id": "navbar", "x": 0, "y": 0, "width": 112, "height": 480,
                    "bg_color": base.CARD, "bg_opa": "COVER", "border_width": 0,
                    "radius": 0, "pad_all": 0, "scrollable": False,
                    "clickable": False, "widgets": [
                        {"label": {"text": "GX", "text_font": "f7_28",
                                   "text_color": base.TXT, "x": 32, "y": 15}},
                        *items]}}


def history_page():
    """Two HA-backed charts below the shared 40-pixel status/settings header."""
    b = base
    w = []
    for prefix, x, width, plot_width, title in (
            ("hh", 120, 360, 288, "TODAY BY HOUR"),
            ("hd", 488, 304, 238, "LAST 7 DAYS")):
        w += [b.rect(x, 44, width, 428, b.CARD, 10),
              b.lbl(x + 16, 56, title, "f7_14", b.TXT2),
              b.lbl(x + 16, 78, "--", "f6_14", b.TXT, id=f"{prefix}_range", w=width - 124),
              b.rect(x + width - 112, 50, 48, 48, b.RAISED, 10,
                     click=b.on_click(b.lam(f"vp::hist_step_5('{prefix[1]}', -1, id(sntp_time).now().timestamp);"),
                                      {"script.execute": "render_page"})),
              b.sym(x + width - 112, 65, "left", 16, b.TXT2, center_w=48),
              b.rect(x + width - 56, 50, 48, 48, b.RAISED, 10,
                     click=b.on_click(b.lam(f"vp::hist_step_5('{prefix[1]}', 1, id(sntp_time).now().timestamp);"),
                                      {"script.execute": "render_page"})),
              b.sym(x + width - 56, 65, "right", 16, b.TXT2, center_w=48)]
        if prefix == "hh":
            for i, cap in enumerate(("HOME", "SOLAR", "GRID", "BATTERY")):
                rx = x + 16 + i * 82
                w += [b.lbl(rx, 108, cap, "f7_12", b.TXT3, id=f"hh_cap{i}"),
                      b.lbl(rx, 126, "--", "f6_20", b.TXT, id=f"hh_val{i}")]
        else:
            for i, cap in enumerate(("SOLAR", "HOME", "IMP", "EXP", "CHG", "DIS")):
                rx = x + 16 + (i % 3) * 90
                ry = 104 + (i // 3) * 26
                w += [b.lbl(rx, ry, cap, "f7_12", b.TXT3, id=f"hd_cap{i}"),
                      b.lbl(rx + 44, ry, "--", "f6_14", b.TXT, id=f"hd_val{i}")]
        w += [b.rect(x + 16, 158, width - 32, 1, b.DIV),
              {"obj": b._id({"x": x + 16, "y": 166, "width": plot_width,
                              "height": 210, "bg_opa": "TRANSP", "border_width": 0,
                              "pad_all": 0, "scrollable": False, "clickable": False},
                             f"{prefix}_chart")},
              b.lbl(x + width - 45, 166, "--", "f6_12", b.TXT3, id=f"{prefix}_scale"),
              b.lbl(x + width - 45, 183, "kWh", "f6_12", b.TXT3),
              b.lbl(x + width - 58, 203, "INPUTS", "f6_12", b.TXT3),
              b.lbl(x + width - 45, 310, "0", "f6_12", b.TXT3, id=f"{prefix}_zero"),
              b.lbl(x + width - 62, 337, "CHG/EXP", "f6_12", b.TXT3,
                    id=f"{prefix}_sinklabel", w=62),
              b.lbl(x + width - 45, 356, "--", "f6_12", b.TXT3, id=f"{prefix}_nscale")]
        if prefix == "hh":
            for i in range(4):
                w.append(b.lbl(x + 16 + i * 72, 384, f"{i * 6:02d}", "f6_12", b.TXT3))
        else:
            for i in range(7):
                w.append(b.lbl(x + 16 + i * 34, 384, "", "f6_12", b.TXT3,
                               id=f"hd_day{i}", w=34, center=True))
        for i, (name, color) in enumerate((("Solar", b.SOLAR), ("Battery", b.BATT),
                                            ("Import", b.IMPORT), ("Export", b.EXPORT))):
            lx = x + 16 + i * (82 if prefix == "hh" else 68)
            key = ("solar", "batt", "imp", "exp")[i]
            w += [b.rect(lx, 444, 9, 9, color, 2, id=f"{prefix}_leg_{key}"),
                  b.lbl(lx + 12, 439, name, "f6_12", b.TXT2,
                        id=f"{prefix}_leg_{key}_lbl")]
        w.append(b.hit(x + 16, 166, plot_width, 210,
                       b.on_click(b.lam(f"vp::hist_tap_5('{prefix[1]}', id(sntp_time).now().timestamp);"),
                                  {"script.execute": "render_page"})))
    return w


def widget_ids(widgets):
    for widget in widgets:
        for cfg in widget.values():
            if isinstance(cfg, dict):
                if cfg.get("id"):
                    yield cfg["id"]
                yield from widget_ids(cfg.get("widgets", []))


HARDWARE = """# Guition JC8048W550 / ESP32-S3, 5-inch 800x480 capacitive display.
# Generated by tools/gen_display_5inch.py. Keep the 4-inch package separate.
output:
  - platform: ledc
    id: backlight_output
    pin: GPIO2
    frequency: 100Hz
    min_power: 0.01
    zero_means_zero: true

light:
  - platform: monochromatic
    name: "Backlight"
    id: display_backlight
    output: backlight_output
    restore_mode: ALWAYS_ON
    default_transition_length: 500ms

i2c:
  id: touch_bus
  sda: GPIO19
  scl: GPIO20

display:
  - platform: rpi_dpi_rgb
    id: tft_display
    color_order: RGB
    invert_colors: true
    update_interval: never
    auto_clear_enabled: false
    dimensions:
      width: 800
      height: 480
    de_pin: GPIO40
    hsync_pin: GPIO39
    vsync_pin: GPIO41
    pclk_pin: GPIO42
    pclk_frequency: 16MHz
    data_pins:
      red: [GPIO45, GPIO48, GPIO47, GPIO21, GPIO14]
      green: [GPIO5, GPIO6, GPIO7, GPIO15, GPIO16, GPIO4]
      blue: [GPIO8, GPIO3, GPIO46, GPIO9, GPIO1]

touchscreen:
  - platform: gt911
    id: tft_touch
    display: tft_display
    on_touch:
      - lambda: 'id(g_display_wake).touch(true, millis());'
      - script.execute: apply_display
    on_release:
      - lambda: 'id(g_display_wake).touch(false, millis());'
      - script.execute: apply_display

"""


def main():
    base.IDS.clear()
    lv = base.build()
    # At 800x480, a 25% RGB565 draw buffer can consume scarce internal RAM.
    # Match the working 5-inch firmware: allocate the full-screen buffer in PSRAM.
    lv["buffer_size"] = "100%"
    for page in lv["pages"]:
        for widget in page["widgets"]:
            resize_tree(widget, root=True)
    # The Wi-Fi page is built at run time inside wf_root: below the 40 px header,
    # right of the 112 px rail, to the bottom edge (688 x 440).
    for page in lv["pages"]:
        for widget in page["widgets"]:
            cfg = widget.get("obj", {})
            if cfg.get("id") == "wf_root":
                cfg.update({"x": OFFSET, "y": 40, "width": 688, "height": 440})
    old_ids = set(widget_ids(lv["pages"][2]["widgets"]))
    base.IDS[:] = [i for i in base.IDS if i not in old_ids]
    lv["pages"][2]["widgets"] = history_page()
    top = lv["top_layer"]["widgets"]
    top[0] = nav_rail()
    for widget in top[1:]:
        resize_tree(widget, root=True)
    # The side rail overlaps no content and retains the original page IDs.
    assert len(base.IDS) == len(set(base.IDS))
    reg = ("auto &u = vp::ui();\n"
           + "\n".join(f"u.{i} = id({i});" for i in base.IDS) + "\n" + base.FONT_REG)
    fonts = base.font_block()
    for font in fonts:
        font["file"] = font["file"].replace("victron/fonts/", "victron_5in/fonts/")
    doc = {"font": fonts, "lvgl": lv,
           "script": [{"id": "ui_register", "then": [base.lam(reg)]}, base.nav_go_script()] + base.wifi_scripts(),
           "globals": [{"id": "g_page", "type": "int", "restore_value": False,
                        "initial_value": "0"},
                       {"id": "g_last_main", "type": "int", "restore_value": False,
                        "initial_value": "0"}]}
    OUT.write_text(HARDWARE + yaml.dump(doc, sort_keys=False, allow_unicode=False,
                                        width=110), encoding="utf-8")
    header = ["// GENERATED by gen_display_5inch.py -- do not edit.",
              "#pragma once", '#include "lvgl.h"', "namespace vp {", "struct UI {"]
    header += [f"  lv_obj_t *{widget_id} = nullptr;" for widget_id in base.IDS]
    header += ["};", "inline UI &ui() { static UI u; return u; }", "}  // namespace vp", ""]
    (ROOT / "victron_5in" / "ui.h").write_text("\n".join(header), encoding="utf-8")
    print(f"Generated {OUT} with {len(base.IDS)} registered widgets")


if __name__ == "__main__":
    main()
