# Flash-and-Address Programming Jig (Driver Board V4)

A 3D-printed clamshell jig that flashes and addresses driver boards with **no soldering per board**. Drop a bare board in, press the head down, and seven spring-loaded pogo pins contact everything needed for a complete "flash → assign address → verify" cycle in one seating.

Design files:
- [jig.scad](jig.scad) — parametric OpenSCAD model (base, press head, pogo test coupon)
- [jig_base.stl](jig_base.stl), [jig_head.stl](jig_head.stl), [jig_coupon.stl](jig_coupon.stl) — pre-rendered with the default parameters, ready to slice (re-export from the .scad if you change `pogo_hole_d` after the coupon test)
- [jig-design.html](jig-design.html) — visual contact layout + cross-section diagrams

---

## Why this works (firmware facts, verified in v7 source)

- `HARDCODED_ID` is written to EEPROM **only on first boot**, guarded by a magic byte (`0x5D` at EEPROM address 0). Every later boot reads the ID from EEPROM.
- The `i` command (`m<oldID>i<newID>`) writes the new ID to EEPROM immediately, so **an address assigned over RS-485 survives reboots**.
- Therefore: flash **one identical binary** onto all boards, then assign each board its unique address over the bus while it sits in the jig. No per-board recompiling.

> In Arduino IDE, check **Tools → Save EEPROM** (megaTinyCore) is set to **retained** — then even reflashing firmware later won't wipe assigned addresses or calibration.

---

## Contact map

Board coordinates measured from the **top-left corner** of the board (component side up, silkscreen text upright), extracted from the V4 KiCad layout. Board outline: **30 × 60 mm**, thickness 1.6 mm. All 7 contact targets are Ø1.0 mm plated through-holes.

| Contact | Signal | X (mm) | Y (mm) | Connects to |
|---------|--------|--------|--------|-------------|
| UPDI header pin 1 | +3V3 | 12.81 | 6.80 | *(unused — board is powered from 12 V)* |
| UPDI header pin 2 | GND | 15.35 | 6.80 | UPDI Friend GND (black) |
| UPDI header pin 3 | UPDI | 17.89 | 6.80 | UPDI Friend UPDI (white) |
| Pogo pad 1 | +12 V | 15.00 | 22.35 | 12 V supply + |
| Pogo pad 2 | RS-485 B | 15.00 | 28.10 | TTL→RS485 converter `B−` |
| Pogo pad 3 | RS-485 A | 15.00 | 31.90 | TTL→RS485 converter `A+` |
| Pogo pad 4 | GND | 15.00 | 37.65 | 12 V supply − |

Registration: **Ø3.0 mm corner holes** at (3, 3), (27, 3), (3, 57), (27, 57). The jig base has pegs at all four.

The RS-485 link comes from the same Pi + isolated TTL→RS485 converter used by the display (see POC_GUIDE §6) — being isolated, it needs only the A/B pair, no ground join. Grounds: UPDI Friend GND joins the 12 V supply −. The Friend's **voltage switch stays on 3V** (logic level reference); its power wire (red) is left unconnected because the board powers itself from 12 V through its own regulators.

## Jig pogo pins

Use **P75-B1 spear-point** spring pins (16.5 mm, ~Ø1.0 mm plunger, ~Ø1.3 mm barrel) — the spear tip self-centers in the board's Ø1 mm holes, which forgives 3D-print tolerance. The module BOM's own 9 mm pogo pins are *not* ideal here (too short to wire comfortably).

Press-fit hole size varies by printer: print the **test coupon** (`part = "coupon"` in the .scad) — a strip of labeled holes from Ø1.0 to Ø1.6 — and set `pogo_hole_d` to the tightest one your pins press into firmly.

## Assembly

1. Print `part = "base"` and `part = "head"` (PETG or PLA, 0.2 mm layers, no supports).
2. Press the 7 pogo pins into the head from below until the barrels seat against the counterbore shoulder; tips should protrude ~4 mm below the head's underside.
3. Solder a wire to each pogo tail inside the top counterbores; route through the channels to the cable exit.
4. Terminate the loom: UPDI + GND → UPDI Friend cable; A, B → the TTL→RS485 converter's `A+`/`B−`; 12 V, GND → barrel-jack screw adapter.
5. Slide the head onto the guide posts. It should fall under its own weight and bottom out on the post shoulders with the pogos compressed ~1.5 mm against a seated board.

## Per-board workflow (~90 seconds each)

1. Seat the board on the corner pegs, component side up. Press the head down.
2. Switch 12 V on. **Wait ~15 s** — with no motor attached, the boot-time homing attempt spins imaginary coils until its safety limit before the module starts listening.
3. **Flash** (first time only per board): *Sketch → Upload Using Programmer* in Arduino IDE. (~10 s)
4. **Address**: only one board is on this bus, so broadcast works — send `m*i7` (for module 7) from `splitflap_test.py` (raw-message option) or any serial terminal at 9600 baud.
5. **Verify**: send `m7d` — the board replies `m7d:<offset>:<steps>:<mapEntries>`. A reply at the new address proves both the flash and the address.
6. 12 V off, lift head, write the module number on the board with a paint pen, next board.

Steps 4–5 are also the whole re-addressing procedure if you ever need to renumber a board later — it works in-system too, as long as the board is alone on the bus or its current address is unique.
