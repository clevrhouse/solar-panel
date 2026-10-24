#!/usr/bin/env python3
"""Generate victron/victron-data.yaml (v2).

The data package only collects state. All drawing happens in panel.h,
called from the render_page script once a second and on every page change.
"""
OUT = "victron/victron-data.yaml"
CHANNELS = {'c_pv_power': 'C_PV', 'c_yield_today': 'C_YIELD', 'c_mppt_bv': 'C_MPPT_V', 'c_mppt_bi': 'C_MPPT_I', 'c_mppt_state': 'C_MPPT_STATE', 'c_batt_soc': 'C_SOC', 'c_batt_v': 'C_BATT_V', 'c_batt_p': 'C_BATT_P', 'c_batt_i': 'C_BATT_I', 'c_batt_ttg': 'C_TTG', 'c_vebus_state': 'C_INV', 'c_pvi_power': 'C_FRONIUS', 'c_minsoc': 'C_MIN', 'c_minsoc_active': 'C_ACTIVE_MIN', 'g_grid_l1': 'C_GRID1', 'g_grid_l2': 'C_GRID2', 'g_grid_l3': 'C_GRID3', 'g_acl_l1': 'C_ACL1', 'g_acl_l2': 'C_ACL2', 'g_acl_l3': 'C_ACL3', 'g_crit_l1': 'C_CRIT1', 'g_crit_l2': 'C_CRIT2', 'g_crit_l3': 'C_CRIT3'}

LIVE_LAMBDA = """          // Decode into a temporary object: a bad packet cannot partially update
          // the panel or make an older packet look current.
          vp::LiveFeed f;
          if (!json::parse_json(x, [&](JsonObject doc) -> bool { return vp::decode_live(doc, f); })) return;
          const uint32_t ms = millis();
          // HA hands the panel its cached attribute when the API (re)connects, which can be
          // minutes old. Age the document by HA's own build time: drop it if it is older than a
          // heartbeat plus slack, and add its age to the GX age. Without a valid clock the age
          // is unknown, so wait for the next heartbeat (at most 20 s) instead of trusting it.
          int64_t packet_age = 0;
          {
            auto t = id(sntp_time).now();
            if (!t.is_valid() || f.at <= 0) return;
            packet_age = (int64_t) t.timestamp - f.at;
            if (packet_age > 45) return;
            if (packet_age < 0) packet_age = 0;
          }
          auto put = [&](vp::Channel c, sensor::Sensor *s, float v) { id(g_fresh).touch(c, ms); s->publish_state(v); };
          put(vp::C_PV, id(c_pv_power), f.pv);
          put(vp::C_YIELD, id(c_yield_today), f.yd);
          put(vp::C_MPPT_V, id(c_mppt_bv), f.mv);
          put(vp::C_MPPT_I, id(c_mppt_bi), f.mi);
          put(vp::C_SOC, id(c_batt_soc), f.soc);
          put(vp::C_BATT_V, id(c_batt_v), f.bv);
          put(vp::C_BATT_P, id(c_batt_p), f.bp);
          put(vp::C_BATT_I, id(c_batt_i), f.bi);
          put(vp::C_TTG, id(c_batt_ttg), f.ttg);
          put(vp::C_FRONIUS, id(c_pvi_power), f.pw);
          put(vp::C_MIN, id(c_minsoc), f.mn);
          put(vp::C_ACTIVE_MIN, id(c_minsoc_active), f.ma);
          id(g_fresh).touch(vp::C_MPPT_STATE, ms);
          id(c_mppt_state).publish_state(f.ms < 0 ? std::string() : vp::mppt_state_name(f.ms));
          id(g_fresh).touch(vp::C_INV, ms);
          id(c_vebus_state).publish_state(f.vs < 0 ? std::string() : vp::vebus_state_name(f.vs));
          id(g_grid_l1) = f.g[0]; id(g_grid_l2) = f.g[1]; id(g_grid_l3) = f.g[2];
          id(g_acl_l1) = f.a[0]; id(g_acl_l2) = f.a[1]; id(g_acl_l3) = f.a[2];
          id(g_crit_l1) = f.c[0]; id(g_crit_l2) = f.c[1]; id(g_crit_l3) = f.c[2];
          id(g_fresh).touch(vp::C_GRID1, ms); id(g_fresh).touch(vp::C_GRID2, ms); id(g_fresh).touch(vp::C_GRID3, ms);
          id(g_fresh).touch(vp::C_ACL1, ms); id(g_fresh).touch(vp::C_ACL2, ms); id(g_fresh).touch(vp::C_ACL3, ms);
          id(g_fresh).touch(vp::C_CRIT1, ms); id(g_fresh).touch(vp::C_CRIT2, ms); id(g_fresh).touch(vp::C_CRIT3, ms);
          id(g_pvi_fronius) = f.fr;
          id(ha_mppt_power).publish_state(f.mp);
          id(ha_ac_pv_power).publish_state(f.ap);
          id(ha_ac_pv_output_power).publish_state(f.ao);
          id(ha_fc_today).publish_state(f.fc[0]);
          id(ha_fc_left).publish_state(f.fc[1]);
          id(ha_fc_this_hour).publish_state(f.fc[2]);
          id(ha_fc_next_hour).publish_state(f.fc[3]);
          id(ha_fc_tomorrow).publish_state(f.fc[4]);
          if (f.fu > 0) id(g_vrm_updated) = (uint32_t) f.fu;
          id(g_cfg) = f.cfg;
          id(g_gx_mq) = f.mq;          // HA's MQTT client is connected
          id(g_gx_age0) = f.ga < 0 ? -1 : f.ga + (int32_t) packet_age;   // seconds since HA last heard from the GX
          id(g_mqtt_last) = ms | 1;    // when this packet arrived"""

VARIANTS = {
    # 4-inch 480x480 panel (victron/) and 5-inch 800x480 panel (victron_5in/)
    "4in": {"out": "victron/victron-data.yaml", "board": "ESP32-S3 480x480",
            "ha": "vp::ha_connected(api::global_api_server, 0)",
            "hist": "vp::HistInput hi{&id(g_history), id(g_history).fresh(now, ms, ch.ha), now, id(g_cfg).bat};"},
    "5in": {"out": "victron_5in/victron-data.yaml", "board": "ESP32-S3 800x480",
            "ha": "vp::ha_connected(api::global_api_server, 0)",
            "hist": "vp::HistInput hi{&id(g_history), id(g_history).fresh(now, ms, ch.ha), now, id(g_cfg).bat};"},
}

def build(variant="4in"):
    v = VARIANTS[variant]
    o = []
    o.append('''# =============================================================================
#  DATA PACKAGE (v3) -- Victron state for the GX panel
#
#  GENERATED by gen_data.py. This file only COLLECTS values; it never touches
#  the screen directly. Everything visual happens in victron/panel.h, called
#  from the `render_page` script once a second and on every page change.
#
#  Everything arrives from Home Assistant over the ESPHome API. There is no MQTT,
#  no portal ID and no per-site setting in this firmware: the Clevr Victron
#  integration reads the Cerbo's MQTT feed in HA and publishes
#    sensor.clevr_victron_live_snapshot     live power, battery, states, topology
#    sensor.clevr_victron_energy_snapshot   today's energy meters
#    sensor.clevr_victron_history_snapshot  24 h / 7 day history
# =============================================================================

''')

    # ---------------------------------------------------------------- sensors
    o.append('''
sensor:
  # Aggregates over every charger / AC inverter, calculated in HA (live snapshot "H").
  - {platform: template, id: ha_mppt_power, internal: true}
  - {platform: template, id: ha_ac_pv_power, internal: true}
  - {platform: template, id: ha_ac_pv_output_power, internal: true}
  # ---- Cerbo values (filled from the live snapshot in text_sensor below)
''')
    for sid in ("c_pv_power", "c_mppt_bv", "c_mppt_bi", "c_batt_soc", "c_batt_v", "c_batt_p",
                "c_batt_i", "c_batt_ttg", "c_pvi_power", "c_minsoc", "c_minsoc_active"):
        o.append(f"  - {{platform: template, id: {sid}, internal: true}}\n")
    o.append('''  - platform: template
    id: c_yield_today
    internal: true
    on_value:
      # The counter dropping means the MPPT started a new solar day: note when.
      - lambda: |-
          if (!std::isnan(x) && id(g_cerbo_yield_prev) >= 0 && x < id(g_cerbo_yield_prev) - 0.05f) {
            auto t = id(sntp_time).now();
            if (t.is_valid()) id(g_cerbo_reset_min) = t.hour * 60 + t.minute;
          }
          if (!std::isnan(x)) id(g_cerbo_yield_prev) = x;

  # ---- direct MPPT readings, published into by the relay package
  - platform: template
    id: b_pv_power
    internal: true
    on_value: [lambda: 'id(g_ble_last) = millis(); id(g_fresh).touch(vp::B_PV, millis());']
  - platform: template
    id: b_yield_today
    internal: true
    on_value:
      - lambda: |-
          id(g_ble_last) = millis();
          id(g_fresh).touch(vp::B_YIELD, millis());
          if (!std::isnan(x) && id(g_ble_yield_prev) >= 0 && x < id(g_ble_yield_prev) - 0.05f) {
            auto t = id(sntp_time).now();
            if (t.is_valid()) id(g_ble_reset_min) = t.hour * 60 + t.minute;
          }
          if (!std::isnan(x)) id(g_ble_yield_prev) = x;
  - platform: template
    id: b_batt_v
    internal: true
    on_value: [lambda: 'id(g_ble_last) = millis(); id(g_fresh).touch(vp::B_V, millis());']
  - platform: template
    id: b_batt_i
    internal: true
    on_value: [lambda: 'id(g_ble_last) = millis(); id(g_fresh).touch(vp::B_I, millis());']

  # Daily energy uses the atomic JSON feed below. Independent subscriptions
  # to last_period and last_reset cannot establish a coherent source period.
''')
    o.append('''
  # ---- solar forecast (live snapshot "F"; found by the integration)
''')
    for sid in ("ha_fc_today", "ha_fc_left", "ha_fc_this_hour", "ha_fc_next_hour", "ha_fc_tomorrow"):
        o.append(f"  - {{platform: template, id: {sid}, internal: true}}\n")
    o.append('''
  # ---- the panel's own link
  - platform: wifi_signal
    id: s_wifi_rssi
    internal: true
    update_interval: 10s

text_sensor:
  - platform: homeassistant
    id: ha_live_snapshot
    entity_id: sensor.clevr_victron_live_snapshot
    attribute: snapshot
    internal: true
    on_value:
      - lambda: |-
@@LIVE@@

  - platform: homeassistant
    id: ha_energy_snapshot
    entity_id: sensor.clevr_victron_energy_snapshot
    attribute: snapshot
    internal: true
    on_value:
      - lambda: |-
          // Decode into a temporary object: a bad packet cannot partially
          // update a meter or keep an older packet looking current.
          vp::EnergyFeed next;
          bool valid = json::parse_json(x, [&](JsonObject doc) -> bool {
            if (!doc["v"].is<int>() || doc["v"].as<int>() != 1 ||
                !doc["at"].is<int64_t>() || !doc["meters"].is<JsonArray>()) return false;
            auto meters = doc["meters"].as<JsonArray>();
            if (meters.size() != vp::METER_COUNT) return false;
            next.observed = (time_t) doc["at"].as<int64_t>();
            for (int i = 0; i < vp::METER_COUNT; ++i) {
              auto row = meters[i];
              if (!row.is<JsonObject>()) return false;
              next.meters[i].today = row["today"].is<float>() ? row["today"].as<float>() : NAN;
              next.meters[i].previous = row["last"].is<float>() ? row["last"].as<float>() : NAN;
              next.meters[i].reset = row["reset"].is<int64_t>() ? (time_t) row["reset"].as<int64_t>() : 0;
            }
            return true;
          });
          next.received = millis();
          next.seen = valid;
          id(g_energy) = next;

  - platform: homeassistant
    id: ha_history_snapshot
    entity_id: sensor.clevr_victron_history_snapshot
    attribute: snapshot
    internal: true
    on_value:
      - lambda: |-
          vp::HistoryFeed next;
          bool valid = json::parse_json(x, [&](JsonObject doc) -> bool {
            if (doc["v"].as<int>() != 1 || !doc["at"].is<int64_t>()) return false;
            next.observed = (time_t) doc["at"].as<int64_t>();
            auto decode = [&](JsonVariant value, auto &rows) -> bool {
              if (!value.is<JsonArray>()) return false;
              auto array = value.as<JsonArray>();
              if (array.size() != rows.size()) return false;
              for (size_t i = 0; i < rows.size(); ++i) {
                if (!array[i].is<JsonArray>() || array[i].as<JsonArray>().size() != vp::METER_COUNT) return false;
                for (int j = 0; j < vp::METER_COUNT; ++j) {
                  auto v = array[i][j];
                  if (v.isNull()) continue;
                  if (!v.is<float>()) return false;
                  float n = v.as<float>();
                  if (!vp::ok(n) || n < 0) return false;
                  rows[i][j] = n;
                }
              }
              return true;
            };
            if (!decode(doc["hours"], next.hours) || !decode(doc["days"], next.days)) return false;
            if (!doc["dates"].is<JsonArray>() || doc["dates"].as<JsonArray>().size() != 8) return false;
            for (int i = 0; i < 8; ++i) {
              if (!doc["dates"][i].is<int64_t>()) return false;
              next.dates[i] = (time_t) doc["dates"][i].as<int64_t>();
              if (next.dates[i] <= 0 || (i && next.dates[i] <= next.dates[i-1])) return false;
            }
            return true;
          });
          next.received = millis(); next.seen = valid;
          id(g_history) = next;

  - {platform: template, id: c_mppt_state, internal: true}
  - {platform: template, id: c_vebus_state, internal: true}
  - platform: template
    id: b_state
    internal: true
    on_value: [lambda: 'id(g_ble_last) = millis(); id(g_fresh).touch(vp::B_STATE, millis());']
  - platform: wifi_info
    ip_address:
      id: ti_wifi_ip
      internal: true

# -----------------------------------------------------------------------------
#  State
# -----------------------------------------------------------------------------
globals:
''')
    for g in ("g_grid", "g_acl", "g_crit"):
        for i in (1, 2, 3):
            o.append(f"  - {{id: {g}_l{i}, type: float, restore_value: false, initial_value: 'NAN'}}\n")
    o.append('''  - {id: g_fresh, type: vp::Freshness, restore_value: false, initial_value: 'vp::Freshness{}'}
  - {id: g_energy, type: vp::EnergyFeed, restore_value: false, initial_value: 'vp::EnergyFeed{}'}
  # Topology from the integration (phases, battery, AC PV, capacity); defaults until the first packet.
  - {id: g_cfg, type: vp::PanelCfg, restore_value: false, initial_value: 'vp::PanelCfg{}'}
  - {id: g_relay_online, type: bool, restore_value: false, initial_value: 'false'}
  - {id: g_mqtt_last, type: uint32_t, restore_value: false, initial_value: '0'}
  - {id: g_gx_mq, type: bool, restore_value: false, initial_value: 'false'}
  - {id: g_gx_age0, type: int32_t, restore_value: false, initial_value: '-1'}
  - {id: g_pvi_fronius, type: bool, restore_value: false, initial_value: 'false'}
  - {id: g_ble_last, type: uint32_t, restore_value: false, initial_value: '0'}
  - {id: g_ha_since, type: uint32_t, restore_value: false, initial_value: '0'}
  - {id: g_vrm_updated, type: uint32_t, restore_value: false, initial_value: '0'}
  - {id: g_ble_yield_prev, type: float, restore_value: false, initial_value: '-1'}
  - {id: g_cerbo_yield_prev, type: float, restore_value: false, initial_value: '-1'}
  - {id: g_ble_reset_min, type: int, restore_value: false, initial_value: '-1'}
  - {id: g_cerbo_reset_min, type: int, restore_value: false, initial_value: '-1'}
  - {id: g_minsoc_pending, type: int, restore_value: false, initial_value: '20'}
  - {id: g_ui_ready, type: bool, restore_value: false, initial_value: 'false'}

  # Disposable history received in full from HA; no local recording.
  - {id: g_history, type: vp::HistoryFeed, restore_value: false, initial_value: 'vp::HistoryFeed{}'}

  # display settings (Settings -> Display), session only; defaults restored on reboot
  - {id: g_display_wake, type: vp::DisplayWake, restore_value: false, initial_value: 'vp::DisplayWake{}'}
  - {id: g_bright, type: int, restore_value: false, initial_value: '80'}
  - {id: g_night, type: bool, restore_value: false, initial_value: 'true'}
  - {id: g_night_lvl, type: int, restore_value: false, initial_value: '10'}
  - {id: g_dim_from, type: int, restore_value: false, initial_value: '1320'}
  - {id: g_dim_until, type: int, restore_value: false, initial_value: '390'}

# -----------------------------------------------------------------------------
#  Rendering
# -----------------------------------------------------------------------------
script:
  - id: render_page
    then:
      - lambda: |-
          if (!id(g_ui_ready)) return;
          auto val = [](sensor::Sensor *s) -> float { return s->has_state() ? s->state : NAN; };
          auto t = id(sntp_time).now();
          time_t now = t.is_valid() ? t.timestamp : 0;
          uint32_t ms = millis();

          // ---- status bar
          vp::Chrome ch;
          ch.now = now;
          ch.rssi = val(id(s_wifi_rssi));
          ch.wifi = wifi::global_wifi_component->is_connected();
          ch.ha = vp::ha_connected(api::global_api_server, 0);
          // Cerbo -> MQTT broker -> HA -> ESPHome API -> panel: each link has its own state.
          ch.received = id(g_mqtt_last) != 0;
          ch.mqtt = ch.ha && id(g_gx_mq);                           // HA's MQTT is connected
          ch.mqtt_seen = ch.received && id(g_gx_age0) >= 0;         // HA has heard from the GX
          ch.mqtt_age_s = ch.mqtt_seen ? (uint32_t) id(g_gx_age0) + (ms - id(g_mqtt_last)) / 1000 : 0;
          vp::render_chrome(ch);
          // Live values are shown only while the whole chain is delivering current data.
          const bool gx_ok = ch.ha && ch.received && ch.mqtt && ch.mqtt_seen && ch.mqtt_age_s <= vp::STALE_S;
          if (ch.ha && id(g_ha_since) == 0) id(g_ha_since) = ms | 1;
          if (!ch.ha) id(g_ha_since) = 0;
          auto live = [&](vp::Channel c, float v) -> float {
            return id(g_fresh).value(c, v, ms, gx_ok);
          };
          auto daily = [&](int i) -> float {
            return vp::meter_today(id(g_energy), i, now, ms, ch.ha);
          };

          // ---- live values, shared by NOW and both overlays
          vp::NowData d;
          const float gp[3] = {live(vp::C_GRID1, id(g_grid_l1)), live(vp::C_GRID2, id(g_grid_l2)), live(vp::C_GRID3, id(g_grid_l3))};
          const float ap[3] = {live(vp::C_ACL1, id(g_acl_l1)), live(vp::C_ACL2, id(g_acl_l2)), live(vp::C_ACL3, id(g_acl_l3))};
          const float cp[3] = {live(vp::C_CRIT1, id(g_crit_l1)), live(vp::C_CRIT2, id(g_crit_l2)), live(vp::C_CRIT3, id(g_crit_l3))};
          d.grid_mask = id(g_cfg).gm; d.acl_mask = id(g_cfg).am; d.crit_mask = id(g_cfg).cm;
          for (int i = 0; i < 3; i++) {
            d.grid[i] = (d.grid_mask & (1u<<i)) ? gp[i] : NAN;
            d.acl[i] = (d.acl_mask & (1u<<i)) ? ap[i] : NAN;
            d.crit[i] = (d.crit_mask & (1u<<i)) ? cp[i] : NAN;
          }
          d.soc = live(vp::C_SOC, val(id(c_batt_soc))); d.batt_w = live(vp::C_BATT_P, val(id(c_batt_p)));
          d.batt_v = live(vp::C_BATT_V, val(id(c_batt_v))); d.batt_a = live(vp::C_BATT_I, val(id(c_batt_i))); d.ttg_s = live(vp::C_TTG, val(id(c_batt_ttg)));
          d.minsoc = live(vp::C_MIN, val(id(c_minsoc))); d.minsoc_active = live(vp::C_ACTIVE_MIN, val(id(c_minsoc_active)));
            d.pvi_count = id(g_cfg).pvn;
            // Aggregates are template sensors that keep their last value: gate them on GX freshness.
            d.fr_w = gx_ok ? val(id(ha_ac_pv_power)) : NAN;
          d.pvi_fronius = id(g_pvi_fronius);
          d.fr_out_w = gx_ok ? val(id(ha_ac_pv_output_power)) : NAN;
          d.mp_w = gx_ok ? val(id(ha_mppt_power)) : NAN;
          d.fr_kwh = daily(vp::D_FRON); d.mp_kwh = daily(vp::D_MPPT);
          std::string inv = id(g_fresh).fresh(vp::C_INV, ms, vp::CERBO_MAX_AGE_MS, gx_ok) ? id(c_vebus_state).state : "";
          for (auto &c : inv) c = toupper(c);
          d.inv = inv.c_str();
          d.cap_kwh = id(g_cfg).cap;
          d.now = now;

          auto &u = vp::ui();
          if (!lv_obj_has_flag(u.ov_phase, LV_OBJ_FLAG_HIDDEN)) vp::render_phase(d);
          if (!lv_obj_has_flag(u.ov_minsoc, LV_OBJ_FLAG_HIDDEN))
            vp::render_minsoc(d, id(g_minsoc_pending), false);

          // ---- today's energy, from the HA meters (same as the energy dashboard)
          vp::Energy e;
          float mp = daily(vp::D_MPPT), fr = daily(vp::D_FRON);
          e.solar = (vp::ok(mp) && vp::ok(fr)) ? mp + fr : NAN;
          e.imp = daily(vp::D_IMP); e.exp = daily(vp::D_EXP);
          e.bin = daily(vp::D_BIN); e.bout = daily(vp::D_BOUT);

          switch (id(g_page)) {
            case 0: vp::render_now(d); break;
            case 1: vp::stale() = false; vp::render_today(e); break;
            case 2: {
              vp::stale() = false;
              vp::HistInput hi{&id(g_history), id(g_history).fresh(now, ms, ch.ha), now};
              vp::render_hist(hi);
              break;
            }
            case 3: {
              vp::stale() = false;
              vp::FcData f;
              f.fc_today = val(id(ha_fc_today)); f.fc_left = val(id(ha_fc_left));
              f.this_h = val(id(ha_fc_this_hour)); f.next_h = val(id(ha_fc_next_hour));
              f.tomorrow = val(id(ha_fc_tomorrow)); f.produced = e.solar;
              if (id(g_history).fresh(now, ms, ch.ha)) {
                f.yesterday = vp::history_energy(id(g_history).days[6]).solar;
                f.avg7 = vp::solar_avg7(id(g_history));
              }
              f.soc = d.soc; f.batt_w = d.batt_w; f.cap_kwh = d.cap_kwh; f.now = now;
              vp::render_forecast(f);
              break;
            }
            case 4: {
              vp::NetData n;
              n.rssi = ch.rssi;
              std::string ip = id(ti_wifi_ip).state;
              n.ip = ip.c_str();
              n.ha = ch.ha; n.ha_up_s = id(g_ha_since) ? (ms - id(g_ha_since)) / 1000 : 0;
              n.mqtt = ch.mqtt; n.mqtt_seen = ch.mqtt_seen; n.mqtt_age_s = ch.mqtt_age_s; n.received = ch.received;
              n.relay_seen = id(g_ble_last) != 0;
              n.relay_age_s = n.relay_seen ? (ms - id(g_ble_last)) / 1000 : 0;
              n.relay = gx_ok && id(g_relay_online) &&
                        id(g_fresh).fresh(vp::B_PV, ms, 90 * 1000U, true);
              n.vrm_updated = id(g_vrm_updated);
              vp::render_net(n);
              break;
            }
            case 5: {
              vp::RecData r;
              bool ble_state_ok = id(g_fresh).fresh(vp::B_STATE, ms, 90 * 1000U, gx_ok && id(g_relay_online));
              bool cerbo_state_ok = id(g_fresh).fresh(vp::C_MPPT_STATE, ms, vp::CERBO_MAX_AGE_MS, gx_ok);
              std::string st = ble_state_ok ? id(b_state).state : (cerbo_state_ok ? id(c_mppt_state).state : "");
              for (auto &c : st) c = toupper(c);
              r.state = st.c_str();
              r.relay_seen = id(g_ble_last) != 0;
              r.relay_age_s = r.relay_seen ? (ms - id(g_ble_last)) / 1000 : 0;
              r.relay = gx_ok && id(g_relay_online) &&
                        id(g_fresh).fresh(vp::B_PV, ms, 90 * 1000U, true);
              const vp::Channel ble[] = {vp::B_PV, vp::B_YIELD, vp::B_V, vp::B_I};
              const vp::Channel cerbo[] = {vp::C_PV, vp::C_YIELD, vp::C_MPPT_V, vp::C_MPPT_I};
              for (int i = 0; i < 4; ++i) {
                r.fresh_b[i] = id(g_fresh).fresh(ble[i], ms, 90 * 1000U, gx_ok && id(g_relay_online));
                r.fresh_c[i] = id(g_fresh).fresh(cerbo[i], ms, vp::CERBO_MAX_AGE_MS, gx_ok);
                r.aligned[i] = id(g_fresh).aligned(ble[i], cerbo[i], ms);
              }
              r.pv_b = val(id(b_pv_power)); r.pv_c = val(id(c_pv_power));
              r.y_b = val(id(b_yield_today)); r.y_c = val(id(c_yield_today));
              r.bv_b = val(id(b_batt_v)); r.bv_c = val(id(c_mppt_bv));
              r.ba_b = val(id(b_batt_i)); r.ba_c = val(id(c_mppt_bi));
              r.reset_b_min = id(g_ble_reset_min); r.reset_c_min = id(g_cerbo_reset_min);
              vp::render_rec(r);
              break;
            }
            case 6:
              vp::render_disp(id(g_bright), id(g_night), id(g_night_lvl), id(g_dim_from), id(g_dim_until));
              break;
            case 7:
              vp::render_dev("${panel_version}", ESPHOME_VERSION, ms / 1000, "ESP32-S3 480x480");
              break;
          }

  # Backlight from the Display settings (brightness + night dimming window).
  - id: apply_display
    then:
      - lambda: |-
          auto t = id(sntp_time).now();
          int target = vp::backlight_target(t.is_valid() ? t.timestamp : 0, id(g_bright), id(g_night),
                                            id(g_night_lvl), id(g_dim_from), id(g_dim_until), id(g_display_wake).awake);
          auto call = id(display_backlight).turn_on();
          call.set_brightness(target / 100.0f);
          call.set_transition_length(300);
          call.set_save(false);
          call.perform();
      - script.execute: render_page

  # Settings -> Device -> RELOAD DATA: have HA rebuild the energy and history
  # snapshots now (they otherwise refresh every minute). Needs 'Allow the device to
  # perform Home Assistant actions'.
  - id: reload_data
    then:
      - homeassistant.action:
          action: homeassistant.update_entity
          data:
            entity_id: sensor.clevr_victron_energy_snapshot
      - homeassistant.action:
          action: homeassistant.update_entity
          data:
            entity_id: sensor.clevr_victron_history_snapshot

  # Min SOC overlay -> SET. The panel never writes to the GX (view only).
  - id: minsoc_set
    then:
      - lvgl.widget.hide: ov_minsoc

# -----------------------------------------------------------------------------
#  Periodic work
# -----------------------------------------------------------------------------
interval:
  # Widget pointers are bound once in esphome.on_boot, after LVGL setup.
  - interval: 1s
    then:
      - if:
          condition:
            lambda: 'return id(g_display_wake).expire(millis());'
          then:
            - script.execute: apply_display
      - script.execute: render_page

  # Once a minute: only the backlight schedule, no energy recording.
  - interval: 60s
    then:
      - script.execute: apply_display
''')
    return finish("".join(o).replace("@@LIVE@@", LIVE_LAMBDA), v)

OLD_HA = "vp::ha_connected(api::global_api_server, 0);"
OLD_HIST = "vp::HistInput hi{&id(g_history), id(g_history).fresh(now, ms, ch.ha), now};"

def swap(text, old, new, count=1):
    assert text.count(old) == count, (old, text.count(old))
    return text.replace(old, new)

CASES = """            case 8: vp::stale() = false; vp::wifi::render(u.wf_root); break;
"""

def finish(text, v):
    """Variant differences plus the Wi-Fi setup additions."""
    text = swap(text, "ch.ha = " + OLD_HA, "ch.ha = " + v["ha"] + ";")
    text = swap(text, OLD_HIST, v["hist"])
    text = swap(text, '"ESP32-S3 480x480"', '"%s"' % v["board"])
    text = swap(text, "          switch (id(g_page)) {",
                "          vp::wifi::tick();   // a join that outlives the Wi-Fi page still finishes\n"
                "          switch (id(g_page)) {")
    text = swap(text, "              n.vrm_updated = id(g_vrm_updated);\n",
                "              n.vrm_updated = id(g_vrm_updated);\n"
                "              n.wifi_up = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();\n"
                "              n.wifi_saved = vp::wifi::have_saved();\n")
    anchor = 'vp::render_dev("${panel_version}", ESPHOME_VERSION, ms / 1000, "%s");\n              break;\n' % v["board"]
    text = swap(text, anchor, anchor + CASES)
    return text

if __name__ == "__main__":
    import argparse
    from pathlib import Path
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", choices=sorted(VARIANTS), default="4in")
    args = ap.parse_args()
    root = Path(__file__).resolve().parents[1]
    path = root / VARIANTS[args.variant]["out"]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(build(args.variant), encoding="utf-8")
    print("written", VARIANTS[args.variant]["out"])
