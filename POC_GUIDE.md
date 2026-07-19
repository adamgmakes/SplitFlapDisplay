# Single-Module Proof of Concept — Beginner Guide

A beginner-friendly companion to [BUILD_GUIDE.md](BUILD_GUIDE.md) for building and testing **one module**, driven directly from a Raspberry Pi's GPIO serial port, with no bus board or DIN rail. Assumes the module is already printed and mechanically assembled (pogo pins omitted).

> A diagram-rich version of this guide exists as a Claude artifact page; this file is the plain-text reference copy.

---

## 1. How it fits together

```
 Raspberry Pi ──4 jumper wires──> TTL→RS-485 module ──A/B wires──> driver board ──> motor ──> drum
 (GPIO UART)                            │
 12V wall adapter ──screw adapter──────> board (+12V and GND)
```

**RS-485** is a hardier two-wire serial link (wires called **A** and **B**). Each message names a module number; your single module will be **module 0**. The Pi already has a serial port on its GPIO header — a small £7 TTL→RS-485 module converts its 3.3 V logic to the bus signals. No USB adapter needed. Everything runs at 12 V — safe to touch; the only fatal-to-the-board mistake is swapping +12 V and GND.

## 2. Shopping list (~£37 from The Pi Hut)

| Item | Purpose |
|---|---|
| Adafruit **UPDI Friend** (£6.70) | Flashes firmware onto the ATtiny1616 — [link](https://thepihut.com/products/adafruit-updi-friend-usb-serial-updi-programmer) |
| **TTL to RS485 Galvanic Isolated Converter (C)** (£6.80) | Pi GPIO ↔ RS-485 bus — [link](https://thepihut.com/products/ttl-to-rs485-c-galvanic-isolated-converter). Auto direction control (no code changes), 3.3 V-safe, isolated |
| **12 V / 2 A power supply**, 2.1 mm barrel jack (£12.50) | Motor power — [link](https://thepihut.com/products/12v-2a-power-supply-with-2-1mm-barrel-jack) |
| **Female DC jack → screw terminal adapter** (~£1.50) | Solder-free power hookup — [link](https://thepihut.com/products/female-dc-power-adapter-2-1mm-jack-to-screw-terminal-block) |
| **22 AWG solid-core wire**, red/black/blue/green spools (£2.80 ea) | The four board wires — [red](https://thepihut.com/products/solid-core-wire-spool-25ft-22awg-red) / [black](https://thepihut.com/products/solid-core-wire-spool-25ft-22awg-black) / [blue](https://thepihut.com/products/solid-core-wire-spool-25ft-22awg-blue) / [green](https://thepihut.com/products/solid-core-wire-spool-25ft-22awg-green) |
| **F-F jumper wires**, 20-pack (~£2) | Pi ↔ RS-485 module, and UPDI Friend ↔ board — [link](https://thepihut.com/products/rpi-premium-jumper-wires-20pk-female-female-100mm) |
| **Female 2.54 mm header**, 3-pin variant (~£1.50) | Plug-in UPDI connection on the board — [link](https://thepihut.com/products/female-2-54mm-header-packs) |

Also needed: a **Raspberry Pi** (any 40-pin model — see BUILD_GUIDE for choosing one) with SD card and PSU, and a computer with Arduino IDE for the one-time firmware flash (the UPDI Friend plugs into it over USB-C).

## 3. Know the board (driver V4)

All connection points are labelled on the silkscreen:

- **Pogo-pin holes** (middle column, top→bottom): `12V`, `B`, `A`, `G` — solder wires directly into these.
- **UPDI Interface** header (top): `+3V3  G  UPDI` — programming, used once.
- **Motor JST-XH** (bottom) and **Hall Sensor** connector (left) — already connected in your build.

## 4. Solder four wires (~15 min)

Cut 4 × ~30 cm solid-core wires. Convention used throughout: **red = 12V, black = G, blue = A, green = B**.

1. Strip 3 mm, insert from the back of the board, solder, tug-test.
2. Strip 6 mm from the free ends for screw terminals.

> ⚠️ **Red must be in `12V`, black in `G`.** Reversed power kills the board. Swapped A/B is harmless (just silent — swap back).

## 5. Flash the firmware (~30 min, mostly one-time setup)

The ATtiny1616 has no USB; the UPDI Friend bridges USB → the 3-pin UPDI header.

**One-time Arduino setup:**
1. Install the Arduino IDE.
2. Settings → Additional boards manager URLs: `http://drazzy.com/package_drazzy.com_index.json`
3. Boards Manager → install **megaTinyCore**.
4. Tools: Board → megaTinyCore → *ATtiny3216/1616/…* · Chip → *ATtiny1616* · Programmer → *SerialUPDI — 230400 baud*. Defaults elsewhere. Also check **Save EEPROM = retained**.

**Set the address:** in `firmware/splitflapfirmwarev7/splitflapfirmwarev7.ino` (~line 40):

```cpp
const uint8_t HARDCODED_ID = 0;   // was 38
```

**Flash:**
1. UPDI Friend voltage switch → **3V** (it powers the chip; keep 12 V unplugged).
2. Connect label-to-label with jumper wires: `VCC→+3V3`, `GND→G`, `UPDI→UPDI`.
3. Plug Friend into the computer, pick its port (`/dev/cu.usbserial-…` on Mac) in Tools → Port.
4. **Sketch → Upload Using Programmer** (not the normal Upload). Wait for "Done uploading", unplug.

## 6. Wire it up (~10 min, screwdriver only)

**Pi → RS-485 module** (4 female-female jumpers onto the GPIO header):

| Pi pin | Signal | Module pin |
|---|---|---|
| Pin 1 | 3V3 | VCC |
| Pin 6 | GND | GND |
| Pin 8 | GPIO14 (TXD) | RXD |
| Pin 10 | GPIO15 (RXD) | TXD |

Note TX crosses to RX and vice versa — each device's "talk" pin goes to the other's "listen" pin.

**Module → board:** blue wire → `A+` terminal, green wire → `B−` terminal. (The module is isolated, so no ground wire is needed on the RS-485 side.)

**Power:** red → screw adapter **+**; black → screw adapter **−** (check the +/− marks). Wall adapter stays unplugged for now.

## 7. Enable the Pi's serial port (one-time)

```bash
sudo raspi-config
# Interface Options → Serial Port →
#   "login shell over serial?"  → No
#   "serial port hardware enabled?" → Yes
sudo reboot
```

The port appears as `/dev/serial0`.

## 8. First power-on

Plug in the 12 V adapter. Module 0 starts immediately: the drum rotates (up to ~1 revolution) and stops on the **blank flap** — that's homing via the drum magnet + Hall sensor. If so, everything works.

## 9. Drive it from the Pi

SSH into the Pi (or use its desktop terminal):

```bash
python3 -m pip install pyserial
python3 firmware/splitflapfirmwarev7/splitflap_test.py \
    --port /dev/serial0 --baud 9600
```

From the menu: **Home**, then **Show character → A**. Each action sends a message like `m0-A` ("module 0, show A") down the A/B pair.

**Tuning (saved permanently to the chip):**
- **Calibrate** (`c`) — module measures its own drum rotation. Run once.
- **Nudge** (`s`) — if every character stops slightly early/late, nudge a few steps until framed.
- Off by *whole flaps*? The flaps are loaded shifted — reseat them so blank shows at home.

## 10. The web app

Same Pi, same wiring:

```bash
python3 -m pip install flask pyserial pytz yfinance requests
```

Edit `frontend/app.py`: serial port (~line 14) → `/dev/serial0`, settings path (~line 16 — replace `/home/gordo` with your username and `mkdir -p ~/splitflap`), and port 80 → 5000 at the bottom (avoids sudo). Run `python3 app.py`, browse `http://<pi>:5000`.

The app assumes 3×15 = 45 modules; your module 0 is the top-left character and ignores the rest. Add modules later by repeating this guide with `HARDCODED_ID` = 1, 2, 3…

## 11. Troubleshooting

| Symptom | Fixes, in order |
|---|---|
| Nothing on power-up | Adapter on? Red/black in right holes? Joints solid? ~12 V across 12V/G |
| Spins ~1½ turns, stops randomly | Hall sensor unplugged, too far from drum, or magnet glued upside-down |
| Buzzes, won't turn | Motor plug half-seated; 5 V motor instead of 12 V; jammed gears |
| Homes fine, no response to test tool | Swap blue/green at the module's A+/B− (the classic swap — safe); confirm serial console disabled in raspi-config; port is `/dev/serial0`, baud 9600; TX/RX jumpers crossed correctly |
| `serial0` missing or permission denied | Re-run raspi-config step; add user to dialout group: `sudo usermod -a -G dialout $USER`, re-login |
| Arduino upload fails | Programmer = SerialUPDI 230400; switch on 3V; wires label-to-label; *Upload Using Programmer* |
| Right letters but drifts | Run Calibrate; check gears for debris |

---

Original design by Adam G Makes — CC BY-NC-SA 4.0.
