<p align="center">
  <img src="guition-4848s040-holder.jpg" width="100%" alt="Guition 4848S040 desk stand: rear, side and detail views, the four assembly steps and the connection summary">
</p>

# Guition 4848S040 desk stand with rear USB-C power

A 3D-printable desk stand for the 4-inch Guition ESP32-4848S040 panel. Power comes in through a USB-C socket on the back of the stand, so no cable sticks out of the side of the display.

- **Clean installation:** power enters at the back, with no side cable.
- **Designed for the Guition 4848S040** 4-inch display.
- **Simple build:** a USB-C breakout board and an MX1.25 cable.
- **Practical and tidy:** a clean, stable desktop stand.

## Files

| File | Part | Size (approx.) |
|---|---|---|
| [`guition_4_inch_holder.stl`](guition_4_inch_holder.stl) | Stand that holds the display | 87 × 41 × 81 mm |
| [`guition_4_inch-_usb_mount-keep-orientation.stl`](guition_4_inch-_usb_mount-keep-orientation.stl) | Clamp that holds the USB-C breakout on the back of the stand | 28 × 9 × 12 mm |
| `guition-4848s040-holder.jpg` | The overview picture above | |

GitHub shows a 3D preview when you click an STL file.

> [!IMPORTANT]
> **Print the USB mount in the orientation it is saved in.** Don't rotate or lay it flat in your slicer. The file is already oriented for printing, which is why its name ends in `keep-orientation`.

## What's in the picture

The top row shows the finished stand from three angles:

- **Rear 3/4 view (assembled):** the display leans back in the stand. The USB mount is screwed to the base, below a small rectangular opening where the cable passes through.
- **Side view (true profile):** the display's real viewing angle, and how deep the base is.
- **Rear detail view:** close-up of the USB-C socket. The breakout board is clamped under the printed mount with two screws, and the red and black wires go up through the opening.

The middle row shows the four assembly steps, described below. The bottom row is the connection summary.

## What you need

- The two printed parts
- A USB-C breakout board with labelled **VCC, D-, D+ and GND** pads
- A 4-pin MX1.25 cable, which plugs into the display
- Two small screws to fix the USB mount to the stand
- A soldering iron
- A 5 V USB-C power supply

## Assembly

### 1. Solder the cable

Solder the 4-pin MX1.25 cable to the USB-C breakout board:

| Breakout pad | Wire |
|---|---|
| **VCC (+5V)** | Red wire, which goes to +5V on the display |
| **GND** | Black wire, which goes to GND on the display |
| D+ | Leave unconnected |
| D- | Leave unconnected |

Only power is used. The USB data lines (D+ and D-) are not connected.

### 2. Route the wire

Pass the cable through the small rectangular hole in the back of the stand. Place the breakout board on the base with the USB-C socket facing backwards, put the printed USB mount over it, and fix it with the two screws.

> [!TIP]
> Route the wire through the stand before you connect it to the display. This keeps the installation clean and tidy.

### 3. Connect the display

The back of the Guition 4848S040 has three small MX1.25 connectors:

| Connector | Pins | Use |
|---|---|---|
| **BAT** | MX1.25, 2-pin | Not used |
| **MX1.25 4P** | +5V, TXD, RXD, GND | **Plug the cable in here** |
| **Speak** | MX1.25, 2-pin | Not used |

Plug the cable into the **4-pin** connector.

> [!WARNING]
> Check the pin order before you power the display. +5V and GND from the breakout must land on the +5V and GND pins of the 4-pin connector. TXD and RXD are not used for power.

### 4. Finished assembly

Place the display in the stand and plug a USB-C power supply into the socket on the back. The display is powered from the rear, and no side cable is visible.

## Connection summary

| USB-C breakout | Display MX1.25 4-pin connector |
|---|---|
| VCC (+5V) | +5V |
| GND | GND |
| D+, D- | Not connected |
| | TXD, RXD: not used |

---

Back to the [main README](../README.md).
