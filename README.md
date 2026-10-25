<h1 align="center">Clevr Energy Display — Victron Edition</h1>

<p align="center">
  A 4-inch and 5-inch touchscreen replacement for the Victron GX display,<br>
  built on Guition ESP32-S3 panels and driven by Home Assistant, MQTT and VRM data.
</p>

<p align="center">
  <img src="docs/images/victron-panel-tour.gif" width="480" alt="4-inch panel cycling through Now, Battery, Today, History, Forecast and Settings">
</p>

<p align="center">
  <a href="https://victron.display.solar">victron.display.solar</a> ·
  <a href="#screens">Screens</a> ·
  <a href="#connection-modes">Connection modes</a> ·
  <a href="#getting-started">Getting started</a> ·
  <a href="#beyond-solar">Beyond solar</a>
</p>

---

<p align="center">
  <img src="docs/images/clevr-gx-display-overview.jpg" width="100%" alt="Clevr Victron GX display replacement: the panel next to two MultiPlus-II inverters, a comparison with the standard Victron display, what it can show, and the compact rear-USB mount">
</p>

The panel shows what a Victron GX display shows (power flow, battery, grid and the MultiPlus state) and adds things the GX cannot: per-phase detail, daily energy totals, hourly and 7-day history, and a solar forecast. It also lets you change the ESS minimum state of charge from the screen.

> [!NOTE]
> Clevr Energy Display is an independent project that works with supported Victron GX systems. It is not an official Victron Energy product.

## Contents

- [Screens](#screens)
- [Hardware](#hardware), including a [3D-printed desk stand](#3d-printed-desk-stand-4-inch)
- [Connection modes](#connection-modes)
- [Getting started](#getting-started)
- [Beyond solar](#beyond-solar): Doorbird, irrigation, rollers and shutters, bedroom panel
- [Support the project](#support-the-project)
- [Project status](#project-status)

## Screens

### Now

<img src="docs/images/victron-4in-now.png" width="400" align="right" alt="Now screen at midday: exporting 3.05 kW, battery charging at 78%">

The live overview. Grid, AC loads and critical loads sit on top, each with an L1/L2/L3 breakdown. The MultiPlus sits in the middle with its charger state, and battery and solar are at the bottom.

- **Animated power flow.** Dots move along each active line in the direction the power flows. The MPPT feeds the MultiPlus DC side; the Fronius is AC-coupled on the AC-out bus, so you can see critical loads being fed by the Fronius, or the Fronius feeding back into the MultiPlus.
- **Grid** shows import or export by sign, colour and label, even when individual phases go in opposite directions.
- **Battery** shows SOC, power, voltage, current, time to full or empty, and the active minimum SOC.
- **Solar** combines MPPT and Fronius, with live power and today's yield for each.

Tap the grid or a load card for the per-phase view. Tap the battery to change the minimum SOC.

<br clear="right">

<table>
  <tr>
    <td align="center"><img src="docs/images/victron-4in-evening.png" width="260" alt="Now screen in the evening, on battery with the grid assisting"><br><sub>Evening: on battery, grid assisting</sub></td>
    <td align="center"><img src="docs/images/victron-4in-min-soc.png" width="260" alt="Battery sheet with the minimum SOC slider"><br><sub>Minimum SOC, written to the GX on SET</sub></td>
    <td align="center"><img src="docs/images/victron-4in-phases.png" width="260" alt="Per-phase view for grid, AC loads and critical loads"><br><sub>Per phase: grid, AC loads, critical</sub></td>
  </tr>
</table>

### Today, History and Forecast

These pages use energy data that Home Assistant records, so they are only available in [Home Assistant mode](#connection-modes).

<table>
  <tr>
    <td align="center"><img src="docs/images/victron-4in-today.png" width="260" alt="Today screen with self-sufficiency, energy in and energy out"><br><sub><b>Today:</b> self-sufficiency, energy in and out, including losses</sub></td>
    <td align="center"><img src="docs/images/victron-4in-history-day.png" width="260" alt="History screen, today by hour"><br><sub><b>History:</b> today by hour</sub></td>
    <td align="center"><img src="docs/images/victron-4in-history-week.png" width="260" alt="History screen, last 7 days"><br><sub><b>History:</b> last 7 days</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/images/victron-4in-forecast.png" width="260" alt="Forecast screen: produced so far, left today, next two days"><br><sub><b>Forecast:</b> left today, this hour, next hour, next 2 days</sub></td>
    <td align="center"><img src="docs/images/victron-4in-settings-mppt.png" width="260" alt="Settings, MPPT tab comparing BLE and Cerbo readings"><br><sub><b>Settings → MPPT:</b> BLE vs Cerbo, yield reset times</sub></td>
    <td></td>
  </tr>
</table>

History bars work like the Home Assistant Energy dashboard: above zero is what the home used (solar, then battery, then grid), below zero is where excess solar went (battery, then export). Systems without a battery get the same charts without the battery parts.

### 5-inch layout

The 5-inch panel uses an 800×480 landscape layout with a side rail. The Now screen gets a dedicated Fronius card, an MPPT card and a daily totals strip.

<p align="center">
  <img src="docs/images/victron-5in-now.png" width="800" alt="5-inch Now screen">
</p>

<table>
  <tr>
    <td align="center"><img src="docs/images/victron-5in-history.png" width="400" alt="5-inch History screen with today and 7 days side by side"><br><sub>History: today and 7 days side by side</sub></td>
    <td align="center"><img src="docs/images/victron-5in-forecast.png" width="400" alt="5-inch Forecast screen"><br><sub>Forecast</sub></td>
  </tr>
</table>

## Hardware

| | 4-inch | 5-inch |
|---|---|---|
| Panel | Guition ESP32-4848S040, 480 × 480 | Guition JC8048W550, 800 × 480 |
| Layout | Square, bottom navigation bar | Landscape, side rail |
| Firmware | 4-inch build | 5-inch build |

Each size needs its own firmware. Renaming the 4-inch YAML does not convert it to 5-inch.

You also need a Victron GX device: a Cerbo GX, Ekrano GX or another Venus OS device.

### 3D-printed desk stand (4-inch)

<img src="4inch-panel-support/guition-4848s040-holder.jpg" width="420" align="right" alt="Guition 4848S040 desk stand with rear USB-C power input">

A printable stand for the 4-inch panel. Power comes in through a USB-C socket on the back of the stand, so no cable sticks out of the side of the display.

It is made of two printed parts plus a USB-C breakout board and a 4-pin MX1.25 cable. Only +5V and GND are connected.

STL files, a parts list and assembly steps are in [`4inch-panel-support/`](4inch-panel-support/).

<br clear="right">

## Connection modes

The two modes use different firmware. Screen size changes the layout; it does not add a data source.

| Capability | Standalone | Home Assistant |
|---|---|---|
| Live solar, loads, grid and battery | Yes | Yes |
| GX connection | Local Modbus/TCP | Home Assistant's MQTT feed, plus the panel's own MQTT/API connections |
| Device discovery | Panel scans the GX Modbus services | Clevr integration discovers the MQTT inventory |
| Today energy summary | No | Yes |
| Hourly and 7-day history | No | From the HA Recorder |
| Forecast | No | Optional, with forecast entities |
| Home Assistant required | No | Yes |

```
Standalone:      Victron devices → GX → Modbus/TCP → Display

Home Assistant:  Victron devices → GX → MQTT → HA integration + Recorder → Display
                                                (live MQTT feed + HA API energy data)
```

No cloud service is needed for monitoring in either mode. A forecast provider, if you add one, may use an online service.

## Getting started

### Home Assistant mode

1. **Prepare MQTT.** Enable MQTT on the GX and connect it to the broker Home Assistant uses.
2. **Install the Clevr integration** in Home Assistant.
3. **Review discovery.** Check the devices the integration found in the GX MQTT feed.
4. **Generate the panel kit** for your panel size.
5. **Compile and flash** the ESPHome firmware to the panel.

### Standalone mode

1. **Enable Modbus/TCP** on the GX.
2. **Choose Victron** on the panel's setup screen.
3. **Connect to Wi-Fi.**
4. **Enter the GX address.**
5. **Review the scan** of Modbus devices and unit IDs, then **Save & Open**.

The full guides are at [victron.display.solar](https://victron.display.solar).

## Beyond solar

The same panels can show more than energy. The features below work today, but they are **not part of this repository**. They are shown here so you can see what the hardware can do.

### Doorbird doorbell

The panel shows the live Doorbird camera feed through Home Assistant's Doorbird integration, and a ring history tab lists recent rings.

<table>
  <tr>
    <td align="center"><img src="docs/images/doorbird-4in-live.jpg" width="260" alt="4-inch Doorbird live view"><br><sub>4-inch: live view</sub></td>
    <td align="center"><img src="docs/images/doorbird-5in-live.jpg" width="440" alt="5-inch Doorbird live view"><br><sub>5-inch: live view</sub></td>
  </tr>
</table>

### Irrigation controller

The panel controls Clevr Irrigation controllers through Home Assistant: status with a countdown, zones, programs, history, plus tools to water a zone now or stop everything. It appears as an extra page in the navigation bar.

<table>
  <tr>
    <td align="center"><img src="docs/images/irrigation-4in-status.png" width="260" alt="4-inch irrigation status while watering"><br><sub>Status while watering</sub></td>
    <td align="center"><img src="docs/images/irrigation-5in-zones.png" width="440" alt="5-inch irrigation zones"><br><sub>Zones</sub></td>
  </tr>
</table>

### Bedroom panel: rollers, shutters and shades

A 4-inch panel for bedrooms. The home screen shows the clock, the weather and a short energy summary. Other tabs control lights, the outside rollers and the inside shades.

- **Outside rollers and shutters** are Shelly-controlled. Use up, stop and down, or drag the window to set an exact position.
- **Inside shades** (2 to 4 per room) can be opened, closed or set to any position in between.
- **Weather forecast** for the next 10 hours and the next 3 days, with a custom set of 34 icons.

<table>
  <tr>
    <td align="center"><img src="docs/images/bedroom-home.png" width="200" alt="Bedroom home screen"><br><sub>Home</sub></td>
    <td align="center"><img src="docs/images/bedroom-rollers.png" width="200" alt="Outside rollers"><br><sub>Outside rollers</sub></td>
    <td align="center"><img src="docs/images/bedroom-shades.png" width="200" alt="Inside shades"><br><sub>Inside shades</sub></td>
    <td align="center"><img src="docs/images/bedroom-forecast.png" width="200" alt="Weather forecast"><br><sub>Forecast</sub></td>
  </tr>
</table>

## Support the project

If the display is useful to you, you can support its development:

<a href="https://buymeacoffee.com/clevrhouse"><img src="https://img.shields.io/badge/Buy%20me%20a%20coffee-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black" alt="Buy me a coffee"></a>

## Project status

The Victron edition is in developer preview. Screens in this README use sample data.

---

<p align="center">
  <sub>By <a href="https://clevr.house">clevr.house</a> · <a href="https://display.solar">display.solar</a></sub>
</p>
