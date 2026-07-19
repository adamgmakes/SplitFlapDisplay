# Split-Flap Display — Complete Build Guide

A step-by-step guide to building the modular, 3D-printed split-flap display in this repository. Each module shows one of 64 characters; modules chain together on an RS485 bus and are driven by a Raspberry Pi running a small web app.

> **Companion video:** https://www.youtube.com/watch?v=-C8_AtxEEQc (Adam G Makes)
> **OnShape model:** https://cad.onshape.com/documents/87c916b33ca5d6492b457485/w/b3e5f0f05f6619e6e7931347/e/582ef2164e20b0aa994708ab

<!-- Guide status:
- [x] Phase 1: Overview, tools, shopping list, PCB ordering
- [x] Phase 2: 3D printing + custom flaps
- [x] Phase 3: Firmware
- [x] Phase 4: Frontend / Raspberry Pi
- [x] Phase 5: Assembly, wiring, calibration, troubleshooting
GUIDE COMPLETE -->

**Build order at a glance:**

1. Order PCBs (longest lead time — do this first)
2. Buy motors, hardware, magnets, pogo pins, Raspberry Pi, PSU
3. 3D print all parts (flaps take the longest)
4. Flash firmware onto each driver board with a unique address
5. Assemble modules
6. Wire the bus and power
7. Set up the Raspberry Pi frontend
8. Home, calibrate, and enjoy

---

## 1. How the system works

Each **module** contains:

- A 3D-printed **character drum** holding 64 flaps (space, A–Z, 0–9, symbols, and 8 solid-colour flaps)
- A **28BYJ-48 12 V stepper motor** turning the drum through a printed gear train
- An **A3144 Hall effect sensor** + a 3×1 mm magnet in the drum for homing (finding flap 0)
- A custom **driver PCB**: ATtiny1616 microcontroller, SN65HVD72 RS485 transceiver, ULN2003A stepper driver, plus 3.3 V and 5 V regulators

All modules share a common **RS485 bus** (12 V, GND, A, B) via bus boards, contacting through pogo pins. A **Raspberry Pi** drives the bus from its GPIO serial port through a small TTL→RS485 converter (or a USB-RS485 dongle — both work), addressing each module individually (addresses 0–44) to tell it which character to show. Each module homes itself with the Hall sensor and steps to the right flap on its own.

The stock frontend assumes a **3-row × 15-column display (45 modules)**, but the protocol works with any number — you can start with a single module and grow.

> ⚠️ **Important:** the driver boards are designed for the **12 V** version of the 28BYJ-48 stepper, not the far more common 5 V version. Check before you buy.

---

## 2. Tools you will need

| Tool | Used for |
|------|----------|
| 3D printer (Bambu Lab profiles included; any FDM printer works) | Enclosure, drum, gears, 64 flaps per module |
| Soldering iron | Through-hole parts on the driver PCB (headers, JST connector, Hall sensor lead), bus wiring |
| **UPDI programmer** | Flashing the ATtiny1616 (no bootloader — see §6.1; a cheap USB-serial adapter + one resistor works) |
| Screwdriver to match your M3 flat-head screws | Assembly |
| Multimeter | Checking 12 V / 5 V / 3.3 V rails and bus continuity |
| Super glue / CA glue | Fixing the homing magnet into the drum |
| Computer with Arduino IDE | Compiling and flashing firmware |
| KiCad (optional) | Only if you want to modify/re-export the PCBs |

---

## 3. Shopping list

Full details in [BOM.md](BOM.md). Multiply per-module items by your display width (up to 45 with the stock frontend).

**Per module — mechanical/electronic:**

| Qty | Part | Notes |
|-----|------|-------|
| 1 | 28BYJ-48 stepper motor, **12 V** | Not the 5 V variant |
| 1 | Driver PCB (assembled) | Ordered from JLCPCB — see §4 |
| 1 | A3144 Hall effect sensor | TO-92 package |
| 1 | 3×1 mm N52 magnet | Glued into the drum for homing |
| 1 | 3-pin JST connector | Hall sensor lead |
| 4 | Pogo pins, 9 mm total (needle head 2.0 mm, tube 5.0 mm, tail 2.0 mm, tail Ø1.0 mm) | Module-to-bus-board contact — https://a.co/d/0a69mnW1 |
| 12 | M3 nuts | |
| 4 | M3 × 8 mm flat head screws | |
| 4 | M3 × 30 mm flat head screws | |
| 4 | M3 × 40 mm flat head screws | |

**Once for the whole display:**

| Qty | Part | Notes |
|-----|------|-------|
| 1 | Raspberry Pi | Any model with USB that runs Python 3 |
| 1 | TTL→RS485 converter (Pi GPIO route, recommended) | e.g. https://thepihut.com/products/ttl-to-rs485-c-galvanic-isolated-converter — auto direction control, 3.3 V-safe, isolated. Alternative: a USB-RS485 dongle (e.g. https://www.adafruit.com/product/5994) |
| 1 | 12 V power supply | Steppers dominate the load; firmware staggers module start-up to soften the surge, but size generously (a 28BYJ-48 draws roughly 0.2 A peak — for 45 modules a 12 V/10 A+ supply is a sensible margin) |
| 1 | C14 panel-mount power inlet + power switch + IEC mains cable | Mains input |
| n | Bus boards | From JLCPCB: `PCBs/bus-board/` (single) or `PCBs/bus-board-5-module/` (5 modules per board) |
| 1 | UPDI programmer | DIY option in §6.1 |
| — | DIN rail (optional) | Modules include DIN rail mounts for clean rack-style mounting |

---

## 4. Step 1 — Order the PCBs (JLCPCB)

Do this first; shipping is usually the longest wait. You need two board types:

- **Driver board** — one per module. Use **`PCBs/driver-v4/`** (the current revision — it fixes an RS485-chip pad issue that caused JLCPCB problems on earlier versions; v2/v3 are deprecated and removed).
- **Bus board** — `PCBs/bus-board/` or `PCBs/bus-board-5-module/` (serves 5 modules per board; fewer boards to chain).

### 4.1 Driver board (with assembly service)

The driver board is meant to be ordered with JLCPCB's **PCBA** service so the SMD parts arrive pre-soldered. Ready-to-upload files are in `PCBs/driver-v4/FAB/`:

- Gerbers: **`FAB.zip`**
- BOM: **`BOM File.xls`**
- Placement: **`CPL File.xlsx`**

1. Go to [jlcpcb.com](https://jlcpcb.com) → **Order Now** and upload `FAB.zip`.
2. Set quantity (minimum 5; order one per module plus spares — PCBA setup cost dominates, extra boards are cheap).
3. Leave settings at default; pick a colour if you like.
4. Toggle **PCB Assembly** on → **Standard PCBA** → assembly side **Top** → Confirm.
5. Upload `BOM File.xls` and `CPL File.xlsx`. Components should auto-match the C-numbers listed in [BOM.md](BOM.md) — review anything flagged.
6. Check quantities against the PCB section of [BOM.md](BOM.md) and order.

> The pin headers, JST stepper connector, and Hall-sensor header are **through-hole** and not placed by PCBA — you solder those yourself when the boards arrive (a few minutes per board).

### 4.2 Bus boards (bare PCB, no assembly)

- **5-module bus board:** upload `PCBs/bus-board-5-module/FAB/5-module-bus-bar.zip` as the Gerber file. No assembly needed.
- **Single bus board:** `PCBs/bus-board/FAB/` contains individual gerber (`.gbr`) and drill (`.drl`) files but no pre-made zip — zip the contents of the `FAB/` folder yourself and upload that.

---

## 5. Step 2 — 3D printing

All printable parts are in `CAD Files/`. The `.3mf` files open directly in Bambu Studio / PrusaSlicer / OrcaSlicer with sane settings embedded.

### 5.1 Structural parts (per module)

| Part | File |
|------|------|
| Enclosure body | `Module Enclosure - Enclosure.3mf` |
| Enclosure cover, top | `Module Enclosure - Enclosure Cover Top.3mf` |
| Enclosure cover, bottom | `Module Enclosure - Enclosure Cover Bottom.3mf` |
| Character drum (main) | `Character Drum - Character Drum Main.3mf` |
| Character drum lip (cap) | `Character Drum - Character Drum Lip.3mf` |
| Motor gear | `Motor Gear.3mf` |
| Center gear | `Center Gear.3mf` |
| Gear plate | `Gear Plate.3mf` |
| DIN rail mount | `DIN Rail Mount.3mf` (alternative: `Din Bracket EasierV2.stl`) |
| Wire retainer | `Wire Retainer.3mf` |

(`Character Drum.3mf` is the combined drum if you prefer to print it as one plate.)

### 5.2 Flaps — the stock 64-character set

`CAD Files/64FlapsWithLetters (parts and bambu print profile)/` contains all 64 flaps as individual `.3mf` files (`Flap1` … `Flap58`, plus `Flap57 - Blank (print 7)` — print **7 copies** of the blank as noted in its filename) and `64FlapsPrintProfile.3mf`, a ready-made Bambu Lab print profile with colours assigned. The character set is:

```
(space) A–Z 0–9 ! @ # $ & ( ) - + = ; " : % ' . , / ? * and 8 solid-colour flaps
```

The coloured flaps (red, orange, yellow, green, blue, purple, white) need the matching filament colours — this is where a multi-material printer (AMS) shines. On a single-colour printer you can print flaps in batches per colour.

Flaps are the bulk of the printing: **64 per module**. Start printing them early and in parallel with waiting for PCBs. `flaps-generator/Flaps_holding_case.stl` is an optional storage case for sorted flaps.

### 5.3 Custom flaps (your own font/characters)

Use the OpenSCAD generator in `flaps-generator/`:

1. Install [OpenSCAD](https://openscad.org) and open `flaps.scad`.
2. Edit the parameters at the top: `chars` (exactly 64 characters, display order), `fonts` / `charFont` (font per character, e.g. `"Consolas:style=bold"`), `fontsize`, per-character tweaks (`charSizeOffset`, `charYposOffset`), and `colorLayer`/`colors` for multi-material.
3. Render (F6) and export STL. For multi-colour inlays: export the body with `MakeFlaps(99)`, then one STL per colour layer (`MakeFlaps(0)`, `MakeFlaps(1)`, …), import them all into your slicer as a single grouped object, and assign a filament to each part.

See `flaps-generator/README.md` and its `examples/` folder for reference screenshots.

---

## 6. Step 3 — Firmware

Firmware lives in `firmware/splitflapfirmwarev7/splitflapfirmwarev7.ino` (use v7; v6 is kept for its RS485 documentation). It targets the **ATtiny1616** on the driver board.

### 6.1 Toolchain and programmer

1. In Arduino IDE, install **megaTinyCore** (Spence Konde) via Boards Manager and select *ATtiny1616*.
2. The ATtiny1616 is programmed over **UPDI** (single wire), not a normal serial bootloader. Options:
   - A dedicated UPDI programmer, or
   - **SerialUPDI**: a $2 USB-serial adapter with a small resistor between TX and RX — guide: https://github.com/SpenceKonde/AVR-Guidance/blob/master/UPDI/jtag2updi.md
3. Connect the programmer to the UPDI pin header on the driver board and use **Sketch → Upload Using Programmer** (with the matching SerialUPDI/jtag2updi programmer selected).

### 6.2 Set each module's address before flashing

Every board needs a unique ID. In the `.ino`:

```cpp
const uint8_t HARDCODED_ID = 38;   // line ~40 — change per board, 0–44
```

Change this value, flash, label the board with its number, repeat. The ID is written to EEPROM on first boot. For the stock 3×15 frontend the mapping is **row-major**: top-left = 0, top-right = 14, second row starts at 15, bottom-right = 44.

(You can also re-address a module later over the bus with the `i` command — e.g. `m38i5` re-addresses module 38 as 5 — handy if you flash them all with the same ID by mistake.)

### 6.3 Tunable constants

| Constant | Default | Meaning |
|----------|---------|---------|
| `stepsFromHallToZero` | `2832` | Steps from Hall trigger to flap 0 — the main per-build tuning value |
| `totalStepsPerRev` | `4096` | Half-steps per drum revolution (the `c` calibrate command measures and stores the real value) |
| `stepDelay` | `1` | ms between half-steps (speed) |

Both offset and steps-per-rev can be set over RS485 at runtime and are stored in EEPROM, so you rarely need to reflash for tuning.

### 6.4 RS485 protocol reference

9600 baud, 8N1, half-duplex. Messages are ASCII: `m<ID><CMD>[data]\n`, where `<ID>` is the module number or `*` for broadcast.

| Command | Example | Purpose |
|---------|---------|---------|
| `-<char>` | `m3-B` | Show a character |
| `+<index>` | `m3+7` | Show flap by index (0–63) |
| `h` | `m*h` | Home to Hall sensor (broadcast homes everything) |
| `c` | `m3c` | Calibrate: measure real steps/revolution, save to EEPROM, reply `m3:<steps>` |
| `d` | `m3d` | Dump EEPROM settings (`homeOffset:totalSteps:mapEntries`) |
| `o<steps>` | `m3o2832` | Set home offset (Hall → flap 0) |
| `t<steps>` | `m3t4096` | Set total steps per revolution |
| `s<steps>` | `m3s10` | Nudge forward and fold into home offset (fine-tuning) |
| `g<step>` | `m3g100` | Go to a raw step position |
| `w<idx>:<pos>` | `m3w5:320` | Write an exact step position for one flap to the EEPROM map |
| `i<newId>` | `m3i7` | Re-address module |
| `a<0\|1>` | `m3a1` | Auto-home on boot (1) or restore last position (0) |
| `e` | `m3e` | Erase the EEPROM position map |

Flap character order (index 0 = space = home):

```
 ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$&()-+=;q:%'.,/?*roygbpw
```

`q` stands in for the double-quote character; the trailing `roygbpw` are the solid-colour flaps (red/orange/yellow/green/blue/purple/white).

### 6.5 Bench-test each module

`firmware/splitflapfirmwarev7/splitflap_test.py` is an interactive menu-driven test client — use it to verify every module before final assembly:

```bash
pip install pyserial
python3 splitflap_test.py --port /dev/ttyUSB0 --baud 9600
```

It exposes every protocol command from a menu (show char, home, calibrate, set offsets, re-address, dump EEPROM…) and prints module replies.

---

## 7. Step 4 — Module assembly

Per module, with the video as the visual reference for orientation of parts:

1. **Solder the through-hole parts** on the driver board: pin headers, the 5-pin JST XH stepper connector, and the 3-pin Hall sensor connector. Fit the 4 pogo pins that carry 12 V/GND/A/B to the bus board.
2. **Glue the 3×1 mm magnet** into its recess in the drum body. Note its orientation — the A3144 is polarity-sensitive; if homing never triggers later, the magnet is upside down.
3. **Fit the Hall sensor** (on its JST lead) into the enclosure mount so it passes the magnet as the drum rotates, and plug it into the driver board.
4. **Build the gear train**: press the motor gear onto the 28BYJ-48 shaft, mount the motor to the enclosure, fit the center gear and gear plate.
5. **Load the 64 flaps** into the drum in character-set order (§6.4) with the drum at its home position, then fit the drum cap (lip). Getting flap order right here saves calibration pain later.
6. **Mount the drum** into the enclosure, fit the driver board, route the motor and Hall leads under the wire retainer.
7. **Close the enclosure** (top and bottom covers) with the M3 screws/nuts, and clip on the DIN rail mount.

Assemble one module completely and bench-test it (§6.5) before mass-producing the rest.

---

## 8. Step 5 — System wiring & power

1. **Mount modules** on DIN rail in your display arrangement (stock frontend: 3 rows × 15 columns).
2. **Fit the bus boards** behind them so each module's 4 pogo pins land on its pads. Chain bus boards together, carrying **12 V, GND, RS485 A, RS485 B** the length of the display.
3. **Wire mains**: C14 inlet → power switch → 12 V PSU. PSU output → bus. Keep mains wiring insulated and strain-relieved.
4. **Connect the Pi**: four jumper wires from the GPIO header to the TTL→RS485 converter (pin 1 3V3 → VCC, pin 6 GND → GND, pin 8 TXD → RXD, pin 10 RXD → TXD — note the crossover), then the converter's A+/B− terminals to the bus A/B. Enable the port once via `sudo raspi-config` → Interface Options → Serial Port (console **No**, hardware **Yes**); it appears as `/dev/serial0`. If nothing responds later, the most common cause is swapped A/B — just swap them.
5. Power the Pi from its own 5 V supply (or a 12 V→5 V buck off the main PSU).

On power-up each module waits `ID × 150 ms` before initialising, so the whole display waking up is deliberately a ripple, not a surge.

---

## 9. Step 6 — Raspberry Pi frontend

The frontend (`frontend/app.py`) is a Flask web app with a playlist system and ~20 built-in "apps" (clock, weather, stocks, countdown, animations…).

```bash
# On the Pi:
pip install flask pyserial pytz yfinance requests
```

Before first run, edit the top of `frontend/app.py`:

- **Line ~14** — serial port: `/dev/serial0` for the GPIO converter route (or `/dev/ttyUSB0` if using a USB dongle)
- **Line ~15** — baud stays `9600`
- **Line ~16** — settings path is hardcoded to `/home/gordo/splitflap/settings.json`; change `gordo` to your username and create the directory (`mkdir -p ~/splitflap`)
- If your display isn't 3×15, the layout is also hardcoded (45 modules, `r * 15 + c` mapping) — adjust to taste

Run it:

```bash
cd frontend
sudo python3 app.py     # sudo because it binds port 80; change the port at the bottom of app.py to 5000+ to run unprivileged
```

Open `http://<pi-ip-address>/` from any device on your network. Useful controls: **Home All** (re-home every module), **Sync All** (pull calibration state from module EEPROMs), settings backup/restore, and the playlist editor.

To start on boot, add a systemd service or a `@reboot` cron entry running the same command.

---

## 10. First power-on, calibration & troubleshooting

### First power-on checklist

1. Power the bus with **one module** attached. Its startup delay is ID×150 ms, then it should home itself (drum spins until the Hall sensor fires, then advances to flap 0 = blank).
2. From the Pi (or any computer with the RS485 dongle), run `splitflap_test.py` and send a character: `m<ID>-A`. The drum should stop on A.
3. Add remaining modules one at a time or in groups, checking each responds at its own address.

### Calibration (per module, once)

1. **`m<ID>c`** — auto-measures the true steps-per-revolution and stores it in EEPROM.
2. Show a known character, e.g. `m<ID>-A`. If it consistently lands a flap or two off, adjust the home offset: **`m<ID>s<steps>`** nudges forward and folds the correction into the stored offset (or set it absolutely with `o<steps>`; default is 2832).
3. For stubborn individual flaps, write exact positions into the EEPROM map with `w<idx>:<pos>`.
4. All of this persists in EEPROM — no reflashing needed.

### Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Nothing responds on the bus | RS485 A/B swapped; wrong serial device (`/dev/serial0` needs the console disabled in raspi-config); TX/RX jumpers not crossed; baud not 9600 |
| One module never responds | Duplicate or wrong ID — connect it alone and re-address with `m*i<newId>` |
| Motor buzzes but doesn't turn | 5 V stepper on a 12 V board (burnt), wiring order on the JST, insufficient PSU |
| Drum spins forever and gives up | Hall sensor not seeing the magnet: magnet upside down, sensor too far from drum, or sensor lead unplugged |
| Characters consistently offset by N flaps | Flaps loaded shifted in the drum, or home offset needs tuning (`s`/`o` commands) |
| Right characters but drifts over time | Steps/rev slightly off — run calibrate (`c`); check gears for skipping/debris |
| Module resets or misses steps under load | Undersized 12 V supply or thin bus power wiring |

### Where to dig deeper

- `firmware/splitflapfirmwarev6/RS485.MD` — protocol background
- `docs/datasheets/` — ATtiny1616, ULN2003A, SN65HVD72, SPX3819 datasheets
- The build video — mechanical assembly close-ups

---

## License

CC BY-NC-SA 4.0 — credit **Adam G Makes**, non-commercial, share-alike. See [README.md](README.md).
