#!/usr/bin/env python3
"""Generate victron/display-guition-4848s040.yaml and victron/ui.h.

Coordinates are the ones measured from the Claude Design render (480x480).
Every widget with an id becomes a field in ui.h, and a registration lambda
binds them, so the YAML and the C++ side cannot drift apart.
"""
import yaml

OUT_YAML = "victron/display-guition-4848s040.yaml"
OUT_H = "victron/ui.h"

# ---------------------------------------------------------------- yaml plumbing
class H(int):  # colour, emitted as 0xRRGGBB
    pass
class Lam(str):  # !lambda
    pass
class Raw(str):  # emitted plain (e.g. SIZE_CONTENT)
    pass

def _h(d, v): return d.represent_scalar("tag:yaml.org,2002:int", "0x%06X" % int(v))
def _l(d, v): return d.represent_scalar("!lambda", str(v), style="|" if "\n" in v else None)
def _r(d, v): return d.represent_scalar("tag:yaml.org,2002:str", str(v))
yaml.add_representer(H, _h)
yaml.add_representer(Lam, _l)
yaml.add_representer(Raw, _r)

# palette (matches panel.h)
BG, CARD, RAISED, DIV = H(0x0A0E14), H(0x141A23), H(0x1E2733), H(0x2A3441)
TXT, TXT2, TXT3, MUTED = H(0xE8EDF3), H(0x9AA6B5), H(0x5E6A7B), H(0x7D8898)
IMPORT, EXPORT, ACL, CRIT = H(0xF0524D), H(0x25A35E), H(0xA5D63F), H(0x22B8CF)
BATT, SOLAR, HOME, ACCENT = H(0xA78BFA), H(0xF5B82E), H(0xB8C4D2), H(0x2F80ED)
SOLAR_DIM, SELF, WHITE, BAD_BG = H(0x4A3A12), H(0x62D98A), H(0xFFFFFF), H(0x3A1414)

SYM = {  # LVGL built-in symbols (need a built-in montserrat font)
    "left": "\uF053", "right": "\uF054", "up": "\uF077", "down": "\uF078",
    "wifi": "\uF1EB", "settings": "\uF013", "close": "\uF00D", "minus": "\uF068",
    "plus": "\uF067", "charge": "\uF0E7", "refresh": "\uF021", "power": "\uF011",
}

IDS = []  # every widget id, in order -> ui.h

def _id(d, i):
    if i:
        d["id"] = i
        IDS.append(i)
    return d

def rect(x, y, w, h, col, r=0, id=None, hidden=False, click=None, children=None, extra=None):
    d = {"x": x, "y": y, "width": w, "height": h, "bg_color": col, "bg_opa": "COVER",
         "radius": r, "border_width": 0, "pad_all": 0, "scrollable": False,
         "clickable": bool(click)}
    if hidden:
        d["hidden"] = True
    if click:
        d.update(click)
    if extra:
        d.update(extra)
    if children:
        d["widgets"] = children
    return {"obj": _id(d, id)}

def lbl(x, y, t, font, col, id=None, right=None, w=None, center=False, hidden=False,
        wrap=False, parent_w=480, extra=None):
    d = {"text": t, "text_font": font, "text_color": col}
    if right is not None:
        d.update({"align": "TOP_RIGHT", "x": right - parent_w, "y": y})
    else:
        d.update({"x": x, "y": y})
    if w is not None:
        d["width"] = w
        d["long_mode"] = "WRAP" if wrap else "CLIP"
    if center:
        d["text_align"] = "CENTER"
    if hidden:
        d["hidden"] = True
    if extra:
        d.update(extra)
    return {"label": _id(d, id)}

def sym(x, y, name, size, col, id=None, box=None, hidden=False, center_w=None):
    d = {"text": SYM[name], "text_font": "montserrat_%d" % size, "text_color": col,
         "x": x, "y": y}
    if box:  # arrowhead sitting in a background box that masks the line
        d.update({"width": box, "height": box, "text_align": "CENTER",
                  "bg_color": BG, "bg_opa": "COVER"})
    if center_w:
        d.update({"width": center_w, "text_align": "CENTER"})
    if hidden:
        d["hidden"] = True
    return {"label": _id(d, id)}

def vu(x, y, vid, vfont, ufont, col, ucol=None, utext=None, uid=None, pad=0, right=None,
       center=None, parent_w=480):
    """Value + unit side by side; the unit follows the value's width."""
    v = {"label": _id({"text": "---", "text_font": vfont, "text_color": col}, vid)}
    u = {"text": utext or "", "text_font": ufont, "text_color": ucol or col}
    if pad:
        u["pad_bottom"] = pad
    u = {"label": _id(u, uid)}
    d = {"width": Raw("SIZE_CONTENT"), "height": Raw("SIZE_CONTENT"), "bg_opa": "TRANSP",
         "border_width": 0, "pad_all": 0, "scrollable": False,
         "clickable": False,
         "layout": {"type": "FLEX", "flex_flow": "ROW", "flex_align_cross": "END", "pad_column": 4},
         "widgets": [v, u]}
    if right is not None:
        d.update({"align": "TOP_RIGHT", "x": right - parent_w, "y": y})
    elif center is not None:
        d.update({"align": "CENTER", "x": center[0] - parent_w // 2, "y": center[1] - 240})
    else:
        d.update({"x": x, "y": y})
    return {"obj": d}

def row_group(right, y, children, parent_w=480):
    """Right-aligned flex row (badge + value)."""
    return {"obj": {"align": "TOP_RIGHT", "x": right - parent_w, "y": y,
                    "width": Raw("SIZE_CONTENT"), "height": Raw("SIZE_CONTENT"),
                    "bg_opa": "TRANSP", "border_width": 0, "pad_all": 0,
                    "scrollable": False, "clickable": False,
                    "layout": {"type": "FLEX", "flex_flow": "ROW", "flex_align_cross": "CENTER",
                               "pad_column": 10},
                    "widgets": children}}

def badge(id, font="f7_12"):
    return {"label": _id({"text": "--", "text_font": font, "text_color": TXT2,
                          "bg_color": RAISED, "bg_opa": "COVER", "radius": 6,
                          "pad_left": 8, "pad_right": 8, "pad_top": 3, "pad_bottom": 3}, id)}

def on_click(*actions):
    return {"on_click": list(actions)}

def show_page(p):
    return {"lvgl.page.show": p}

def lam(code):
    return {"lambda": Lam(code)}

def caption(x, y, t, font="f7_12", col=TXT3, **kw):
    return lbl(x, y, t, font, col, **kw)

# ---------------------------------------------------------------- fonts
FONTS = {  # id: (weight file, size)
    "f5_12": ("Medium", 12), "f5_14": ("Medium", 14),
    "f6_12": ("SemiBold", 12), "f6_14": ("SemiBold", 14), "f6_16": ("SemiBold", 16),
    "f6_18": ("SemiBold", 18), "f6_20": ("SemiBold", 20), "f6_24": ("SemiBold", 24),
    "f6_32": ("SemiBold", 32),
    "f7_12": ("Bold", 12), "f7_14": ("Bold", 14), "f7_16": ("Bold", 16),
    "f7_18": ("Bold", 18), "f7_20": ("Bold", 20), "f7_28": ("Bold", 28),
    "f7_40": ("Bold", 40), "f7_48": ("Bold", 48),
}
DIGITS = "0123456789-+.%: "  # the big hero fonts only ever show numbers
# Wi-Fi setup (wifi_ui.h): every printable ASCII character, plus a bullet for hidden passwords.
ASCII = "".join(chr(c) for c in range(32, 127)) + "\u2022"
WIFI_FONTS = {"wr12": ("Medium", 12), "wr14": ("Medium", 14), "wr16": ("Medium", 16),
              "wb16": ("Bold", 16), "wb20": ("Bold", 20)}

# Material Design Icons subset (victron/fonts/mdi-wifi.ttf, built from
# materialdesignicons-webfont.ttf, Pictogrammers Free License). The code points must match
# MDI_WIFI_GLYPHS in victron/wifi_ui.h.
MDI_CODEPOINTS = ("F05A9 F05AA F091F F0922 F0925 F0928 F16B5 F033E F0208 F0209 F0B5C F0311 F0636 F0A7A "
                  "F012C F005D F0045 F0415 F0141 F0450").split()
MDI_FONT_FILE = "victron/fonts/mdi-wifi.ttf"

def font_block():
    out = []
    for fid, (w, s) in FONTS.items():
        f = {"file": "victron/fonts/Montserrat-%s.ttf" % w, "id": fid, "size": s, "bpp": 4}
        if s >= 40:
            f["glyphs"] = [DIGITS]
        out.append(f)
    for fid, (w, s) in WIFI_FONTS.items():
        out.append({"file": "victron/fonts/Montserrat-%s.ttf" % w, "id": fid, "size": s, "bpp": 4,
                    "glyphs": [ASCII]})
    glyphs = [chr(int(cp, 16)) for cp in MDI_CODEPOINTS]
    for fid, size in (("mdi_24", 24), ("mdi_48", 48)):
        out.append({"file": MDI_FONT_FILE, "id": fid, "size": size, "bpp": 4, "glyphs": list(glyphs)})
    return out

# Font and hook registration for the run-time built Wi-Fi setup page (wifi_ui.h).
FONT_REG = """{
  auto &wf = vp::wifi::fonts();
  wf.r12 = id(wr12)->get_lv_font(); wf.r14 = id(wr14)->get_lv_font(); wf.r16 = id(wr16)->get_lv_font();
  wf.b16 = id(wb16)->get_lv_font(); wf.b20 = id(wb20)->get_lv_font();
  wf.i24 = id(mdi_24)->get_lv_font(); wf.i48 = id(mdi_48)->get_lv_font();
}
vp::wifi::link::install();
vp::wifi::hooks().open = []() { id(wifi_open)->execute(); };
vp::wifi::hooks().back = []() { id(wifi_back)->execute(); };
vp::wifi::init_icon(u.conn_wifi);
vp::wifi::boot();"""

# ---------------------------------------------------------------- NOW page
def power_card_widgets(x, title, pfx, col, state_id=None):
    w = [rect(x, 40, 148, 120, CARD, 10),
         lbl(x + 12, 51, title, "f6_14", TXT2)]
    if state_id:
        w.append(lbl(x + 61, 53, "", "f7_12", TXT3, id=state_id))
    w.append(vu(x + 12, 74, pfx + "_total", "f6_32", "f6_16", col, uid=pfx + "_unit", pad=4))
    for i, dx in enumerate((12, 53, 95)):
        w.append(caption(x + dx, 119, "L%d" % (i + 1), "f6_12", id="%s_cap%d" % (pfx, i + 1)))
        w.append(lbl(x + dx, 135, "---", "f6_14", TXT, id="%s_l%d" % (pfx, i + 1)))
    return w

def arrow(x, y, id):
    return sym(x, y, "right", 12, TXT3, id=id, box=14, hidden=True)

def page_now():
    w = []
    w += power_card_widgets(8, "GRID", "grid", TXT2, state_id="grid_state")
    w += power_card_widgets(166, "AC LOADS", "acl", ACL)
    w += power_card_widgets(324, "CRITICAL", "crit", CRIT)
    # flow lines. AC-in side: grid + AC loads. AC-out side: critical + Fronius.
    # DC side: battery + MPPT. (design correction #1)
    w += [rect(81, 160, 2, 50, DIV, id="fl_grid_a"), rect(81, 208, 85, 2, DIV, id="fl_grid_b"),
          rect(239, 160, 2, 14, DIV, id="fl_acl_a"), rect(83, 172, 158, 2, DIV, id="fl_acl_b"),
          rect(314, 208, 85, 2, DIV, id="fl_out"), rect(397, 160, 2, 50, DIV, id="fl_crit"),
          rect(397, 208, 45, 2, DIV, id="fl_fr_h"), rect(440, 208, 2, 48, DIV, id="fl_fr_v"),
          rect(199, 232, 2, 24, DIV, id="fl_batt"), rect(279, 232, 2, 24, DIV, id="fl_mppt"),
          arrow(117, 202, "ar_grid"), arrow(153, 166, "ar_acl"), arrow(349, 202, "ar_out"),
          arrow(434, 225, "ar_fr"), arrow(193, 237, "ar_batt"), arrow(273, 237, "ar_mppt")]
    # MultiPlus
    w += [rect(166, 184, 148, 48, RAISED, 10),
          lbl(166, 190, "MULTIPLUS", "f6_12", TXT3, w=148, center=True),
          lbl(166, 207, "---", "f7_18", TXT, id="inv_state", w=148, center=True)]
    # battery
    w += [rect(8, 256, 226, 160, CARD, 10),
          lbl(20, 266, "BATTERY", "f6_14", TXT2),
          lbl(0, 269, "---", "f7_12", TXT2, id="batt_state", right=222),
          vu(20, 289, "batt_soc", "f7_48", "f6_24", TXT, utext="%", uid="batt_pct", pad=4),
          vu(132, 297, "batt_pw", "f6_24", "f6_14", MUTED, uid="batt_pw_u", pad=2),
          lbl(132, 327, "", "f5_14", TXT2, id="batt_eta"),
          rect(20, 360, 202, 12, RAISED, 6),
          rect(20, 360, 0, 12, BATT, 6, id="batt_bar", hidden=True),
          rect(59, 356, 2, 20, ACCENT, id="batt_min_mk", hidden=True),
          lbl(20, 387, "---", "f5_14", TXT2, id="batt_va"),
          lbl(0, 388, "MIN --- %", "f7_12", ACCENT, id="batt_minsoc", right=212),
          sym(215, 388, "right", 12, ACCENT)]
    # solar: both arrays, one card
    w += [rect(246, 256, 226, 160, CARD, 10),
          lbl(258, 266, "SOLAR", "f6_14", TXT2),
          lbl(0, 268, "--.-- kWh today", "f6_14", TXT2, id="solar_today", right=460),
          vu(258, 290, "solar_total", "f6_32", "f6_16", MUTED, uid="solar_unit", pad=4),
          rect(258, 336, 202, 1, DIV),
          # Name set at runtime: "Fronius" for one identified inverter, "AC Solar" for multiple.
          lbl(258, 340, "PV Inverter", "f6_14", TXT, id="fr_name"),
          lbl(258, 357, "", "f6_12", TXT3, id="fr_kwh"),
          lbl(0, 345, "---", "f6_16", MUTED, id="fr_w", right=460),
          rect(258, 372, 202, 1, DIV),
          lbl(258, 376, "MPPT", "f6_14", TXT),
          lbl(258, 393, "", "f6_12", TXT3, id="mp_kwh"),
          lbl(0, 381, "---", "f6_16", MUTED, id="mp_w", right=460)]
    # tap targets, on top
    open_phase = on_click({"lvgl.widget.show": "ov_phase"}, {"script.execute": "render_page"})
    w += [hit(8, 40, 148, 120, open_phase), hit(166, 40, 148, 120, open_phase),
          hit(324, 40, 148, 120, open_phase),
          hit(8, 256, 226, 160, on_click(lam("id(g_minsoc_pending) = id(c_minsoc).has_state() && !std::isnan(id(c_minsoc).state) ? (int) id(c_minsoc).state : 20;"),
                                         {"lvgl.widget.show": "ov_minsoc"},
                                         {"script.execute": "render_page"}))]
    return w

def hit(x, y, w, h, click):
    d = {"x": x, "y": y, "width": w, "height": h, "bg_opa": "TRANSP", "border_width": 0,
         "pad_all": 0, "scrollable": False, "clickable": True}
    d.update(click)
    return {"obj": d}

# ---------------------------------------------------------------- TODAY page
def legend(x, y, col, t):
    return [rect(x, y + 3, 8, 8, col, 2), caption(x + 14, y, t)]

def page_today():
    w = [rect(8, 40, 464, 136, CARD, 10),
         {"arc": _id({"x": 12, "y": 42, "width": 128, "height": 128,
                      "start_angle": 135, "end_angle": 45, "min_value": 0, "max_value": 100,
                      "value": 0, "arc_width": 12, "arc_color": RAISED, "arc_rounded": True,
                      "adjustable": False, "clickable": False,
                      "indicator": {"arc_color": SELF, "arc_width": 12, "arc_rounded": True},
                      "knob": {"bg_opa": "TRANSP", "pad_all": 0}}, "td_arc")},
         vu(0, 0, "td_selfsuff", "f7_28", "f6_14", TXT, utext="%", pad=3, center=(76, 104)),
         lbl(24, 150, "SELF-SUFF.", "f6_12", TXT3, w=104, center=True),
         caption(152, 56, "HOME USED TODAY"),
         vu(152, 74, "td_home", "f6_32", "f6_16", TXT, ucol=TXT2, utext="kWh", pad=4),
         caption(152, 124, "SOLAR"),
         lbl(152, 142, "--- kWh", "f6_16", SOLAR, id="td_solar"),
         caption(304, 124, "GRID NET"),
         lbl(304, 142, "---", "f6_16", TXT3, id="td_gridnet")]
    for (y, title, tid, segs, items) in (
            (184, "ENERGY IN", "in_total", ("in_seg0", "in_seg1", "in_seg2"),
             ((SOLAR, "SOLAR", "in_solar"), (BATT, "BATTERY", "in_batt"), (IMPORT, "GRID", "in_grid"))),
            (304, "ENERGY OUT", "out_total", ("out_seg0", "out_seg1", "out_seg2"),
             ((HOME, "HOME", "out_home"), (BATT, "BATTERY", "out_batt"), (EXPORT, "EXPORT", "out_export")))):
        w += [rect(8, y, 464, 112, CARD, 10),
              lbl(24, y + 13, title, "f6_14", TXT2),
              lbl(0, y + 13, "--- kWh", "f6_14", TXT, id=tid, right=456),
              rect(24, y + 38, 432, 16, RAISED, 4)]
        for sid, (col, _, _) in zip(segs, items):
            w.append(rect(24, y + 38, 0, 16, col, 4, id=sid, hidden=True))
        for i, (col, cap, vid) in enumerate(items):
            x = 24 + 144 * i
            w += legend(x, y + 65, col, cap)
            w.append(vu(x, y + 82, vid, "f6_20", "f6_12", TXT, ucol=TXT3, utext="kWh", pad=1))
    return w

# ---------------------------------------------------------------- HISTORY page
def page_hist():
    w = [rect(8, 40, 160, 44, RAISED, 8, id="hist_seg_today", click=on_click(lam("vp::hist_mode(false);"), {"script.execute":"render_page"})),
         lbl(8, 54, "TODAY BY HOUR", "f7_14", TXT, id="hist_lbl_today", w=160, center=True),
         rect(176, 40, 152, 44, CARD, 8, id="hist_seg_days", click=on_click(lam("vp::hist_mode(true);"), {"script.execute":"render_page"})),
         lbl(176, 54, "LAST 7 DAYS", "f7_14", TXT2, id="hist_lbl_days", w=152, center=True)]
    for prefix, chart in (("hh", "h"), ("hd", "d")):
        c = [rect(8, 88, 464, 328, CARD, 10), lbl(24, 98, "--", "f6_14", TXT, id=prefix+"_range"),
             rect(368, 90, 44, 40, RAISED, 8, click=on_click(lam(f"vp::hist_step_4('{chart}', -1, id(sntp_time).now().timestamp);"), {"script.execute":"render_page"})),
             sym(368, 102, "left", 16, TXT2, center_w=44),
             rect(420, 90, 44, 40, RAISED, 8, click=on_click(lam(f"vp::hist_step_4('{chart}', 1, id(sntp_time).now().timestamp);"), {"script.execute":"render_page"})),
             sym(420, 102, "right", 16, TXT2, center_w=44)]
        caps = ("HOME", "SOLAR", "GRID", "BATTERY") if chart == "h" else ("SOLAR", "HOME", "IMP", "EXP", "CHG", "DIS")
        for i, cap in enumerate(caps):
            x = 24 + (i % (4 if chart=="h" else 3)) * (104 if chart=="h" else 140)
            y = 139 + (0 if chart=="h" else i//3*24)
            c += [lbl(x,y,cap,"f7_12",TXT3,id=f"{prefix}_cap{i}"),lbl(x+(0 if chart=="h" else 46),y+(17 if chart=="h" else 0),"--","f6_14",TXT,id=f"{prefix}_val{i}")]
        c += [{"obj":_id({"x":24,"y":192,"width":384,"height":172,"bg_opa":"TRANSP","border_width":0,"pad_all":0,"scrollable":False,"clickable":False}, prefix+"_chart")},
              lbl(419,192,"--","f6_12",TXT3,id=prefix+"_scale"),lbl(419,210,"kWh","f6_12",TXT3),
              lbl(412,232,"INPUT","f6_12",TXT3),lbl(419,272,"0","f6_12",TXT3,id=prefix+"_zero"),
              lbl(411,326,"CHG/EXP","f6_12",TXT3,id=prefix+"_sinklabel"),lbl(419,346,"--","f6_12",TXT3,id=prefix+"_nscale")]
        if chart=="h":
            for i in range(4):c.append(lbl(24+i*96,365,f"{i*6:02d}","f6_12",TXT3))
        else:
            for i in range(7):c.append(lbl(24+i*54,365,"", "f6_12",TXT3,id=f"hd_day{i}",w=54,center=True))
        for i,(key,cap,col) in enumerate((("solar","Solar",SOLAR),("batt","Battery",BATT),("imp","Import",IMPORT),("exp","Export",EXPORT))):
            x=24+i*104
            c += [rect(x,399,9,9,col,2,id=f"{prefix}_leg_{key}"),lbl(x+12,394,cap,"f6_12",TXT2,id=f"{prefix}_leg_{key}_lbl")]
        c.append(hit(24,192,384,172,on_click(lam(f"vp::hist_tap_4('{chart}', id(sntp_time).now().timestamp);"), {"script.execute":"render_page"})))
        w.append(rect(0,0,480,416,BG,id=prefix+"_panel",hidden=chart=="d",children=c,extra={"bg_opa":"TRANSP"}))
    return w

# ---------------------------------------------------------------- FORECAST page
def page_fc():
    w = [rect(8, 40, 464, 176, CARD, 10),
         lbl(24, 52, "TODAY", "f6_14", TXT2),
         lbl(0, 53, "FORECAST --- kWh", "f7_12", TXT3, id="fc_forecast", right=456),
         caption(24, 78, "PRODUCED"),
         vu(24, 96, "fc_prod", "f6_32", "f6_16", SOLAR, utext="kWh", pad=4),
         caption(0, 78, "LEFT TODAY", right=456),
         vu(0, 96, "fc_left", "f6_32", "f6_16", TXT, ucol=TXT2, utext="kWh", pad=4, right=456),
         rect(24, 144, 432, 16, SOLAR_DIM, 5),
         rect(24, 144, 0, 16, SOLAR, 5, id="fc_bar", hidden=True),
         rect(308, 140, 2, 24, TXT, id="fc_tick", hidden=True),
         caption(24, 173, "VS FORECAST"), lbl(24, 190, "---", "f6_16", TXT3, id="fc_vs"),
         caption(168, 173, "THIS HOUR"), lbl(168, 190, "---", "f6_16", TXT, id="fc_this"),
         caption(312, 173, "NEXT HOUR"), lbl(312, 190, "---", "f6_16", TXT, id="fc_next")]
    for n, x in (("d1", 8), ("d2", 244)):
        w += [rect(x, 224, 228, 104, CARD, 10),
              lbl(x + 14, 237, "", "f7_12", TXT2, id="fc_%s_name" % n),
              lbl(0, 237, "", "f7_12", TXT2, id="fc_%s_rating" % n, right=x + 214),
              vu(x + 14, 256, "fc_%s_kwh" % n, "f6_32", "f6_16", SOLAR, utext="kWh", pad=4),
              rect(x + 14, 298, 200, 8, RAISED, 4),
              rect(x + 14, 298, 0, 8, SOLAR, 4, id="fc_%s_bar" % n, hidden=True),
              rect(x + 113, 295, 2, 14, TXT, id="fc_%s_tick" % n, hidden=True),
              lbl(x + 14, 310, "", "f5_14", TXT3, id="fc_%s_avg" % n)]
    w += [rect(8, 336, 464, 80, CARD, 10),
          sym(24, 350, "charge", 16, SOLAR),
          lbl(56, 349, "", "f6_16", TXT, id="fc_ins_title"),
          lbl(56, 371, "", "f5_14", TXT2, id="fc_ins_text", w=400, wrap=True)]
    return w

# ---------------------------------------------------------------- SETTINGS pages
TABS = (("NETWORK", "pg_s_net"), ("MPPT", "pg_s_mppt"), ("DISPLAY", "pg_s_disp"), ("DEVICE", "pg_s_dev"))
PG_WIFI = 8
# Main (non-settings) pages in close-button order: page code -> page id.
MAIN_PAGES = ((0, "pg_now"), (1, "pg_today"), (2, "pg_hist"), (3, "pg_fc"))

def tabbar(active):
    w = []
    for i, (t, pg) in enumerate(TABS):
        x = 8 + 117 * i
        a = i == active
        w.append(rect(x, 40, 113, 48, RAISED if a else CARD, 10,
                      click=None if a else on_click(show_page(pg))))
        w.append(lbl(x, 57, t, "f7_12", TXT if a else TXT2, w=113, center=True))
        if a:
            w.append(rect(x + 36, 82, 40, 3, ACCENT, 2))
    return w

def setting_row(y, h, cap, right_widget):
    return [rect(8, y, 464, h, CARD, 10), lbl(24, y + h // 2 - 8, cap, "f5_14", TXT2), right_widget]

def page_s_net():
    w = tabbar(0)
    rows = (("WiFi signal", "net_wifi_badge", "net_wifi"), ("IP address", None, "net_ip"),
            ("Home Assistant", "net_ha_badge", "net_ha"), ("GX data", "net_mqtt_badge", "net_mqtt"),
            ("BLE relay", "net_relay_badge", "net_relay"), ("VRM forecast", None, "net_vrm"))
    kids = []
    for i, (cap, bid, vid) in enumerate(rows):
        y = 50 * i   # relative to net_rows, which starts at y=96
        val = lbl(0, 0, "---", "f6_16", TXT, id=vid)
        rw = row_group(456, y + 12, ([badge(bid)] if bid else []) + [val])
        kids += setting_row(y, 46, cap, rw)
    # The first row opens the saved Wi-Fi networks.
    kids.append(hit(8, 0, 464, 46, on_click(lam("vp::wifi::request_list();"), show_page("pg_wifi"))))
    w.append(rect(0, 96, 480, 296, BG, id="net_rows", children=kids, extra={"bg_opa": "TRANSP"}))
    # Nothing saved and not connected: the status rows mean nothing, so show only this message.
    w += [lbl(0, 150, chr(0xF05AA), "mdi_48", TXT3, id="net_empty_ic", w=480, center=True, hidden=True),
          lbl(16, 214, "No Wi-Fi network saved", "f7_20", TXT, id="net_empty_t", w=448, center=True, hidden=True),
          lbl(32, 248, "Add your home network so this panel can reach Home Assistant.", "f5_14", TXT2,
              id="net_empty_s", w=416, wrap=True, center=True, hidden=True)]
    # Always at the bottom, also after a network was saved.
    w.append(rect(8, 404, 464, 52, ACCENT, 10, id="net_add",
                  click=on_click(lam("vp::wifi::request_add();"), show_page("pg_wifi")),
                  children=[{"label": {"text": "+  ADD WI-FI", "text_font": "wb16", "text_color": WHITE,
                                       "align": "CENTER"}}]))
    return w

def page_s_mppt():
    w = tabbar(1)
    w += [rect(8, 96, 464, 56, CARD, 10),
          lbl(24, 106, "SMARTSOLAR MPPT", "f7_14", TXT),
          lbl(24, 126, "Charger state ---", "f5_12", TXT2, id="rec_state"),
          row_group(456, 102, [badge("rec_relay")]),
          lbl(0, 127, "", "f5_12", TXT3, id="rec_seen", right=456),
          caption(0, 162, "BLE", right=236), caption(0, 162, "CERBO", right=336),
          caption(0, 162, "DELTA", right=456)]
    rows = (("PV power", "pv"), ("Yield today", "y"), ("Battery V", "bv"), ("Battery A", "ba"))
    for i, (cap, k) in enumerate(rows):
        y = 184 + 52 * i
        w += [rect(8, y, 464, 48, CARD, 10), lbl(24, y + 16, cap, "f5_14", TXT),
              lbl(0, y + 15, "---", "f6_16", TXT, id="rec_%s_b" % k, right=236),
              lbl(0, y + 15, "---", "f6_16", TXT, id="rec_%s_c" % k, right=336),
              lbl(0, y + 16, "--", "f7_14", TXT2, id="rec_%s_d" % k, right=456)]
    w += [rect(8, 390, 464, 1, DIV),
          caption(24, 398, "YIELD RESET"),
          lbl(0, 405, "--:--", "f6_14", TXT2, id="rec_rs_b", right=236),
          lbl(0, 405, "--:--", "f6_14", TXT2, id="rec_rs_c", right=336),
          lbl(0, 405, "--", "f7_14", TXT2, id="rec_rs_d", right=456),
          lbl(24, 440, "BLE column comes straight off the charger via the relay, not through the Cerbo.",
              "f5_12", TXT3, w=432, wrap=True)]
    return w

def stepper(y, cap, vid, dec, inc):
    return [rect(8, y, 464, 64, CARD, 10), lbl(24, y + 24, cap, "f5_14", TXT2),
            rect(296, y + 8, 48, 48, RAISED, 8, click=on_click(lam(dec), {"script.execute": "apply_display"})),
            sym(296, y + 22, "minus", 20, TXT2, center_w=48),
            lbl(344, y + 22, "--", "f6_18", TXT, id=vid, w=72, center=True),
            rect(416, y + 8, 48, 48, RAISED, 8, click=on_click(lam(inc), {"script.execute": "apply_display"})),
            sym(416, y + 22, "plus", 20, TXT2, center_w=48)]

def page_s_disp():
    w = tabbar(2)
    w += [rect(8, 96, 464, 96, CARD, 10),
          lbl(24, 110, "Brightness", "f5_14", TXT2),
          lbl(0, 106, "--%", "f6_20", TXT, id="disp_bright_val", right=456),
          {"slider": _id({"x": 24, "y": 156, "width": 432, "height": 8, "min_value": 5,
                          "max_value": 100, "value": 80, "bg_color": RAISED, "radius": 4,
                          "indicator": {"bg_color": ACCENT, "radius": 4},
                          "knob": {"bg_color": TXT, "pad_all": 10, "radius": 14},
                          "on_value": [lam("id(g_bright) = (int) x;"),
                                       {"script.execute": "apply_display"}]},
                         "disp_bright")},
          rect(8, 200, 464, 56, CARD, 10),
          lbl(24, 219, "Night dimming", "f5_14", TXT2),
          {"switch": _id({"x": 400, "y": 212, "width": 56, "height": 32,
                          "bg_color": RAISED,
                          "indicator": {"checked": {"bg_color": ACCENT}},
                          "knob": {"bg_color": TXT, "pad_all": -4},
                          "on_value": [lam("id(g_night) = x;"), {"script.execute": "apply_display"}]},
                         "disp_night")}]
    w += stepper(264, "Night level", "disp_lvl",
                 "id(g_night_lvl) = std::max(5, id(g_night_lvl) - 5);",
                 "id(g_night_lvl) = std::min(100, id(g_night_lvl) + 5);")
    w += stepper(336, "Dim from", "disp_from",
                 "id(g_dim_from) = (id(g_dim_from) + 1440 - 30) % 1440;",
                 "id(g_dim_from) = (id(g_dim_from) + 30) % 1440;")
    w += stepper(408, "Dim until", "disp_until",
                 "id(g_dim_until) = (id(g_dim_until) + 1440 - 30) % 1440;",
                 "id(g_dim_until) = (id(g_dim_until) + 30) % 1440;")
    return w

def page_s_dev():
    w = tabbar(3)
    for i, (cap, vid) in enumerate((("Firmware", "dev_fw"), ("ESPHome", "dev_esph"),
                                    ("Uptime", "dev_up"), ("Free PSRAM", "dev_psram"),
                                    ("Board", "dev_board"))):
        y = 96 + 52 * i
        w += setting_row(y, 48, cap, lbl(0, y + 15, "---", "f6_16", TXT, id=vid, right=456))
    flexc = {"layout": {"type": "FLEX", "flex_flow": "ROW", "flex_align_main": "CENTER",
                        "flex_align_cross": "CENTER", "flex_align_track": "CENTER",
                        "pad_column": 8}}
    w += [rect(8, 376, 228, 56, RAISED, 10, extra=flexc,
               click=on_click({"script.execute": "reload_data"}),
               children=[{"label": {"text": SYM["refresh"], "text_font": "montserrat_16", "text_color": TXT}},
                         {"label": {"text": "RELOAD DATA", "text_font": "f7_14", "text_color": TXT}}]),
          rect(244, 376, 228, 56, BAD_BG, 10, extra=flexc,
               click={"on_long_press": [lam("App.safe_reboot();")]},
               children=[{"label": {"text": SYM["power"], "text_font": "montserrat_16", "text_color": IMPORT}},
                         {"label": {"text": "HOLD TO RESTART", "text_font": "f7_14", "text_color": IMPORT}}]),
          lbl(8, 440, "Restart needs a long press, so a stray tap can't reboot the panel.",
              "f5_12", TXT3, w=464, center=True)]
    return w


def page_wifi():
    # Built at run time by vp::wifi::render (wifi_ui.h), sized from this container:
    # 480x440 here, 688x440 on the 5-inch panel. A settings-style page (no bottom bar).
    return [rect(0, 40, 480, 440, BG, id="wf_root")]

def wifi_scripts():
    return [{"id": "wifi_open", "then": [show_page("pg_wifi")]},
            {"id": "wifi_back", "then": [show_page("pg_s_net")]}]

# ---------------------------------------------------------------- overlays (top layer)
def close_btn(target):
    return [rect(424, 4, 48, 48, RAISED, 10, click=on_click({"lvgl.widget.hide": target})),
            sym(424, 18, "close", 20, TXT2, center_w=48)]

def overlay_phase():
    c = [lbl(16, 10, "PER PHASE", "f7_16", TXT), lbl(16, 33, "", "f6_12", TXT2, id="ph_imb")]
    c += close_btn("ov_phase")
    for n in (1, 2, 3):
        y = 48 + 96 * (n - 1)
        c += [rect(8, y, 464, 88, CARD, 10),
              lbl(22, y + 33, "L%d" % n, "f7_20", TXT),
              caption(80, y + 10, "GRID"),
              lbl(120, y + 11, "", "f7_12", TXT3, id="ph_l%d_state" % n),
              lbl(80, y + 27, "---", "f6_24", TXT3, id="ph_l%d_grid" % n),
              rect(80, y + 64, 192, 8, RAISED, 4),
              rect(176, y + 64, 0, 8, IMPORT, 2, id="ph_l%d_bar" % n, hidden=True),
              rect(175, y + 60, 2, 16, TXT3),
              caption(292, y + 10, "AC LOADS"),
              lbl(292, y + 30, "---", "f6_20", ACL, id="ph_l%d_acl" % n),
              caption(384, y + 10, "CRITICAL"),
              lbl(384, y + 30, "---", "f6_20", CRIT, id="ph_l%d_crit" % n)]
    y = 344
    c += [rect(8, y, 464, 96, RAISED, 10), lbl(22, y + 40, "TOTAL", "f7_14", TXT),
          caption(80, y + 10, "GRID"), lbl(120, y + 11, "", "f7_12", TXT3, id="ph_t_state"),
          lbl(80, y + 27, "---", "f6_24", TXT3, id="ph_t_grid"),
          caption(292, y + 10, "AC LOADS"), lbl(292, y + 30, "---", "f6_20", ACL, id="ph_t_acl"),
          caption(384, y + 10, "CRITICAL"), lbl(384, y + 30, "---", "f6_20", CRIT, id="ph_t_crit")]
    return rect(0, 32, 480, 448, BG, id="ov_phase", hidden=True, click={}, children=c,
                extra={"clickable": True})

def overlay_minsoc():
    step = lambda d: on_click(lam("id(g_minsoc_pending) = std::max(0, std::min(100, id(g_minsoc_pending) %+d));" % d),
                              {"script.execute": "render_page"})
    c = [lbl(16, 10, "BATTERY", "f7_16", TXT), lbl(16, 33, "---", "f7_12", TXT2, id="ms_state")]
    c += close_btn("ov_minsoc")
    c += [vu(16, 61, "ms_soc", "f7_48", "f6_24", TXT, utext="%", pad=4),
          vu(200, 63, "ms_pw", "f6_24", "f6_14", MUTED, uid="ms_pw_u", pad=2),
          lbl(200, 95, "", "f5_14", TXT2, id="ms_va"),
          lbl(0, 95, "", "f5_14", TXT2, id="ms_note", right=464),
          rect(16, 132, 448, 20, RAISED, 6),
          rect(16, 132, 0, 20, BATT, 6, id="ms_bar", hidden=True),
          rect(105, 128, 2, 28, TXT, id="ms_mk_cur", hidden=True),
          rect(148, 126, 4, 32, ACCENT, 2, id="ms_mk_new"),
          lbl(16, 160, "0%", "f6_12", TXT3), lbl(0, 160, "100%", "f6_12", TXT3, right=464),
          caption(16, 190, "MINIMUM SOC (ESS)"),
          lbl(0, 190, "", "f6_12", TXT3, id="ms_active", right=464),
          vu(16, 208, "ms_val", "f7_40", "f7_40", ACCENT, utext="%"),
          lbl(130, 233, "", "f6_14", TXT2, id="ms_was"),
          rect(16, 264, 56, 56, RAISED, 10, click=step(-5)),
          sym(16, 282, "minus", 20, TXT, center_w=56),
          {"slider": _id({"x": 88, "y": 288, "width": 304, "height": 8, "min_value": 0,
                          "max_value": 100, "value": 20, "bg_color": RAISED, "radius": 4,
                          "indicator": {"bg_color": ACCENT, "radius": 4},
                          "knob": {"bg_color": TXT, "pad_all": 12, "radius": 16},
                          "on_value": [lam("id(g_minsoc_pending) = ((int) x + 2) / 5 * 5;"),
                                       {"script.execute": "render_page"}]}, "ms_slider")},
          rect(408, 264, 56, 56, RAISED, 10, click=step(5)),
          sym(408, 282, "plus", 20, TXT, center_w=56),
          lbl(88, 324, "0", "f6_12", TXT3), lbl(227, 324, "50", "f6_12", TXT3),
          lbl(372, 324, "100", "f6_12", TXT3),
          rect(16, 348, 216, 56, RAISED, 10, click=on_click({"lvgl.widget.hide": "ov_minsoc"}),
               children=[{"label": {"text": "CANCEL", "text_font": "f7_14", "text_color": TXT,
                                    "align": "CENTER"}}]),
          rect(248, 348, 216, 56, ACCENT, 10, id="ms_set",
               click=on_click({"script.execute": "minsoc_set"}),
               children=[{"label": _id({"text": "SET", "text_font": "f7_14", "text_color": WHITE,
                                        "align": "CENTER"}, "ms_set_lbl")}]),
          lbl(16, 412, "", "f5_12", TXT3, id="ms_help", w=448, wrap=True)]
    return rect(0, 32, 480, 448, BG, id="ov_minsoc", hidden=True, click={}, children=c,
                extra={"clickable": True})

def nav_go_script():
    """Bottom bar / rail buttons call nav_go(slot): NOW, TODAY, HISTORY, FORECAST."""
    then = []
    for slot, (code, pg) in enumerate(MAIN_PAGES):
        then.append({"if": {"condition": {"lambda": Lam("return slot == %d;" % slot)},
                            "then": [show_page(pg)]}})
    return {"id": "nav_go", "parameters": {"slot": "int"}, "then": then}

def navbar():
    kids = []
    for i, (t, pg) in enumerate((("NOW", "pg_now"), ("TODAY", "pg_today"),
                                 ("HISTORY", "pg_hist"), ("FORECAST", "pg_fc"))):
        kids.append(rect(8 + 117 * i, 4, 113, 48, RAISED, 10, id="nav_bg%d" % i,
                         click=on_click({"script.execute": {"id": "nav_go", "slot": i}}),
                         children=[{"label": _id({"text": t, "text_font": "f7_14", "text_color": TXT2,
                                                  "align": "CENTER", "y": -2}, "nav_lbl%d" % i)},
                                   {"obj": _id({"align": "BOTTOM_MID", "y": -3, "width": 41, "height": 3,
                                                "bg_color": ACCENT, "bg_opa": "COVER", "radius": 2,
                                                "border_width": 0, "clickable": False,
                                                "hidden": True}, "nav_ul%d" % i)}]))
    return {"obj": _id({"x": 0, "y": 424, "width": 480, "height": 56, "bg_color": BG,
                        "bg_opa": "COVER", "border_width": 0, "radius": 0, "pad_all": 0,
                        "scrollable": False, "clickable": False, "widgets": kids}, "navbar")}

def statusbar():
    close_actions = [show_page("pg_now")]
    return [
        lbl(12, 8, "--:--", "f6_14", TXT2, id="sb_clock"),
        lbl(12, 7, "SETTINGS", "f7_14", TXT, id="set_title", hidden=True),
        row_group(424, 5, [
            {"label": _id({"text": SYM["wifi"], "text_font": "montserrat_14", "text_color": TXT3}, "conn_wifi")},
            {"label": _id({"text": "HA", "text_font": "f7_12", "text_color": TXT3}, "conn_ha")},
            {"label": _id({"text": "GX", "text_font": "f7_12", "text_color": TXT3,
                           "bg_color": RAISED, "bg_opa": "TRANSP", "radius": 4,
                           "pad_left": 6, "pad_right": 6, "pad_top": 2, "pad_bottom": 2}, "conn_mqtt")}]),
        hit(432, 0, 48, 40, on_click(show_page("pg_s_net"))),
        sym(432, 7, "settings", 18, TXT2, id="ico_settings", center_w=48),
        rect(432, 0, 48, 40, BG, id="btn_close", hidden=True, click=on_click(*close_actions)),
        sym(432, 8, "close", 18, TXT2, id="ico_close", hidden=True, center_w=48),
    ]

# ---------------------------------------------------------------- assemble
def page(pid, idx, widgets, enter=None):
    return {"id": pid, "bg_color": BG, "scrollable": False,
            "on_load": [lam("id(g_page) = %d;%s if (id(g_ui_ready)) vp::render_nav(%d);" %
                            (idx, " id(g_last_main) = %d;" % idx if idx in (0, 1, 2, 3) else "", idx)),
                        ] + ([lam(enter)] if enter else []) + [{"script.execute": "render_page"}],
            "widgets": widgets}

def build():
    pages = [page("pg_now", 0, page_now()), page("pg_today", 1, page_today()),
             page("pg_hist", 2, page_hist()), page("pg_fc", 3, page_fc()),
             page("pg_s_net", 4, page_s_net()), page("pg_s_mppt", 5, page_s_mppt()),
             page("pg_s_disp", 6, page_s_disp()), page("pg_s_dev", 7, page_s_dev()),
             page("pg_wifi", PG_WIFI, page_wifi(), enter="vp::wifi::entered();")]
    # settings button hit area gets an id so render_nav can hide it
    sb = statusbar()
    sb[3]["obj"] = _id(sb[3]["obj"], "btn_settings")
    top = [navbar(), overlay_phase(), overlay_minsoc()] + sb
    # Register the line widget dependency used by the C++ history chart.
    top.append({"line": {"points": [{"x": 0, "y": 0}, {"x": 1, "y": 1}],
                          "hidden": True}})
    lvgl = {"displays": ["tft_display"],
            "touchscreens": [{"touchscreen_id": "tft_touch", "long_press_time": "1500ms"}],
            "buffer_size": "25%", "bg_color": BG, "default_font": "f6_14",
            "top_layer": {"widgets": top}, "pages": pages}
    return lvgl

HARDWARE = """# =============================================================================
#  DISPLAY PACKAGE -- Guition ESP32-S3-4848S040  (ST7701S, 480x480, GT911)
#
#  GENERATED by gen_display.py from the Claude Design layout. Do not edit by
#  hand: change the generator and re-run it. It also writes victron/ui.h, the
#  list of widget pointers the C++ renderer uses, so the two never drift.
#
#  Knows nothing about Victron. All values are pushed in by victron-data.yaml
#  through victron/panel.h.
# =============================================================================

output:
  - platform: ledc
    id: backlight_output
    pin: GPIO38
    frequency: 150Hz
    min_power: 0.01
    zero_means_zero: true

light:
  - platform: monochromatic
    name: "Backlight"
    id: display_backlight
    output: backlight_output
    restore_mode: ALWAYS_ON
    default_transition_length: 500ms

spi:
  - id: lcd_spi
    clk_pin: GPIO48
    mosi_pin: GPIO47

i2c:
  id: touch_bus
  sda: GPIO19
  scl:
    number: GPIO45
    ignore_strapping_warning: true

display:
  - platform: st7701s
    id: tft_display
    spi_mode: MODE3
    data_rate: 2MHz
    color_order: RGB
    invert_colors: false
    dimensions:
      width: 480
      height: 480
    cs_pin: GPIO39
    de_pin: GPIO18
    hsync_pin: GPIO16
    vsync_pin: GPIO17
    pclk_pin: GPIO21
    pclk_frequency: 12MHz
    pclk_inverted: false
    hsync_pulse_width: 8
    hsync_front_porch: 10
    hsync_back_porch: 20
    vsync_pulse_width: 8
    vsync_front_porch: 10
    vsync_back_porch: 10
    update_interval: never
    auto_clear_enabled: false
    init_sequence:
      - 1
      - [0xFF, 0x77, 0x01, 0x00, 0x00, 0x10]
      - [0xCD, 0x00]
    data_pins:
      red: [11, 12, 13, 14, 0]
      green: [8, 20, 3, 46, 9, 10]
      blue: [4, 5, 6, 7, 15]

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
    lv = build()
    doc = {"font": font_block(), "lvgl": lv}
    # registration: bind every id to the ui.h struct field of the same name
    reg = ("auto &u = vp::ui();\n" + "\n".join("u.%s = id(%s);" % (i, i) for i in IDS)
           + "\n" + FONT_REG)
    doc["script"] = [{"id": "ui_register", "then": [lam(reg)]}, nav_go_script()] + wifi_scripts()
    doc["globals"] = [{"id": "g_page", "type": "int", "restore_value": False, "initial_value": "0"},
                      {"id": "g_last_main", "type": "int", "restore_value": False, "initial_value": "0"}]
    body = yaml.dump(doc, sort_keys=False, allow_unicode=False, width=110, default_flow_style=False)
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    (root / OUT_YAML).parent.mkdir(parents=True, exist_ok=True)
    (root / OUT_YAML).write_text(HARDWARE + body, encoding="utf-8")
    dup = {i for i in IDS if IDS.count(i) > 1}
    assert not dup, dup
    h = ["// GENERATED by gen_display.py -- do not edit.",
         "// One pointer per widget id in display-guition-4848s040.yaml.",
         "#pragma once", '#include "lvgl.h"', "namespace vp {", "struct UI {"]
    for i in IDS:
        h.append("  lv_obj_t *%s = nullptr;" % i)
    h += ["};", "inline UI &ui() { static UI u; return u; }", "}  // namespace vp", ""]
    (root / OUT_H).write_text("\n".join(h), encoding="utf-8")
    print("widgets with ids:", len(IDS))

if __name__ == "__main__":
    main()
