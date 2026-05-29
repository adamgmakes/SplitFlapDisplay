#!/usr/bin/env python3
"""
splitflap_test.py — Interactive RS-485 test tool for split-flap display modules.

Sends commands to one or more modules over a serial RS-485 bus and prints any
responses received.  All commands match the firmware v7 protocol:

  m<ID>-<char>        Show a character          e.g. m38-B
  m<ID>+<index>       Show flap by index         e.g. m38+7
  m<ID>h              Home the module
  m<ID>c              Calibrate (measure revolution)
  m<ID>d              Dump EEPROM config
  m<ID>o<n>           Set home offset            e.g. m38o2832
  m<ID>t<n>           Set total steps/rev        e.g. m38t4096
  m<ID>s<n>           Nudge N steps              e.g. m38s10
  m<ID>g<n>           Go to raw step position    e.g. m38g512
  m<ID>w<idx>:<pos>   Write calibrated position  e.g. m38w7:320
  m<ID>i<n>           Set module ID              e.g. m38i5
  m<ID>a<0|1>         Set auto-home (0=off,1=on) e.g. m38a1
  m<ID>e              Erase position map

  Use * (or **) as ID to broadcast to all modules.

Usage:
  python3 splitflap_test.py [--port /dev/ttyUSB0] [--baud 9600]

  If --port is omitted, USB serial adapters are auto-detected. When several
  are connected, a numbered list is shown so you can pick one.

Requirements:
  pip install pyserial
"""

import argparse
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("ERROR: pyserial not installed.  Run:  pip install pyserial")
    sys.exit(1)

# ── ANSI colours (disabled automatically on Windows or when not a tty) ────────
USE_COLOUR = sys.stdout.isatty() and sys.platform != "win32"

def _c(code, text):
    return f"\033[{code}m{text}\033[0m" if USE_COLOUR else text

def green(t):  return _c("32", t)
def yellow(t): return _c("33", t)
def cyan(t):   return _c("36", t)
def red(t):    return _c("31", t)
def bold(t):   return _c("1",  t)
def dim(t):    return _c("2",  t)

# ── Character set (must match FLAP_CHARS in firmware) ─────────────────────────
FLAP_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$&()-+=;q:%'.,/?*roygbpw"

def flap_index(ch):
    """Return the index of a character in the flap set, or -1 if not present."""
    return FLAP_CHARS.find(ch)

def flap_char(index):
    """Return the character at a given flap index, or '?' if out of range."""
    if 0 <= index < len(FLAP_CHARS):
        return FLAP_CHARS[index]
    return "?"

# ── Serial helpers ─────────────────────────────────────────────────────────────

def _is_usb_port(port_info):
    """Return True if a list_ports entry looks like a USB serial adapter."""
    desc = (port_info.description or "").upper()
    hwid = (port_info.hwid or "").upper()
    device = port_info.device
    if "USB" in desc or "USB" in hwid:
        return True
    if device.startswith("/dev/ttyUSB") or device.startswith("/dev/ttyACM"):
        return True
    if sys.platform == "win32" and device.upper().startswith("COM"):
        return True
    if sys.platform == "darwin" and (
        device.startswith("/dev/cu.usb") or device.startswith("/dev/tty.usb")
    ):
        return True
    return False

def _port_label(port_info):
    """Human-readable one-line description for a serial port."""
    parts = [port_info.device]
    if port_info.description and port_info.description != "n/a":
        parts.append(port_info.description)
    if port_info.manufacturer:
        parts.append(f"({port_info.manufacturer})")
    return " — ".join(parts)

def detect_usb_ports():
    """Return USB-looking serial ports, falling back to all ports if none match."""
    all_ports = list(list_ports.comports())
    usb_ports = [p for p in all_ports if _is_usb_port(p)]
    return usb_ports if usb_ports else all_ports

def choose_serial_port():
    """
    Auto-detect a serial port.  Use the sole match when only one is found;
    otherwise show a numbered list and ask the user to choose.
    """
    ports = detect_usb_ports()
    if not ports:
        print(red("✗ No serial ports found. Connect a USB adapter or pass --port."))
        sys.exit(1)

    if len(ports) == 1:
        port = ports[0].device
        print(green(f"✓ Auto-detected port: {_port_label(ports[0])}"))
        return port

    print(bold("Multiple serial ports found:"))
    for i, port_info in enumerate(ports, 1):
        print(f"  {cyan(str(i)):>4}  {_port_label(port_info)}")

    while True:
        try:
            raw = input(bold(f"Select port [1–{len(ports)}]: ")).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            sys.exit(1)

        if raw == "":
            continue
        try:
            choice = int(raw)
        except ValueError:
            print(red("  Enter a number from the list"))
            continue

        if 1 <= choice <= len(ports):
            port = ports[choice - 1].device
            print(green(f"✓ Using {_port_label(ports[choice - 1])}"))
            return port

        print(red(f"  Please enter a number between 1 and {len(ports)}"))

def open_port(port, baud):
    """Open the serial port; exit with a clear message on failure."""
    try:
        ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,        # short read timeout for the listener thread
        )
        print(green(f"✓ Opened {port} at {baud} baud"))
        return ser
    except serial.SerialException as e:
        print(red(f"✗ Could not open {port}: {e}"))
        sys.exit(1)

def send(ser, message):
    """Send a message string (appends \\n if missing) and print what was sent."""
    if not message.endswith("\n"):
        message += "\n"
    ser.write(message.encode("ascii"))
    ser.flush()
    print(cyan(f"  → {repr(message.strip())}"))

# ── Background listener ────────────────────────────────────────────────────────

_listener_running = False

def start_listener(ser):
    """
    Spawn a daemon thread that prints any bytes arriving from the bus.
    Modules only transmit in response to 'c' (calibrate) and 'd' (dump) commands.
    """
    global _listener_running
    _listener_running = True

    def _listen():
        buf = ""
        while _listener_running:
            try:
                raw = ser.read(64)
            except serial.SerialException:
                break
            if raw:
                buf += raw.decode("ascii", errors="replace")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if line:
                        print(yellow(f"\n  ← {line}"))
                        print(bold("command> "), end="", flush=True)

    t = threading.Thread(target=_listen, daemon=True)
    t.start()

def stop_listener():
    global _listener_running
    _listener_running = False

# ── Command builders ───────────────────────────────────────────────────────────

def build_message(module_id, cmd):
    """Prefix a command string with the module address."""
    return f"m{module_id}{cmd}"

def prompt_id():
    """Ask the user for a module ID (number or * for broadcast)."""
    raw = input(bold("  Module ID (number or * for broadcast): ")).strip()
    if raw == "" or raw == "**":
        return "*"
    return raw

def prompt_int(label, lo=None, hi=None):
    """Ask for an integer, optionally within a range.  Returns None on blank."""
    while True:
        raw = input(bold(f"  {label}: ")).strip()
        if raw == "":
            return None
        try:
            val = int(raw)
            if lo is not None and val < lo:
                print(red(f"  Must be ≥ {lo}"))
                continue
            if hi is not None and val > hi:
                print(red(f"  Must be ≤ {hi}"))
                continue
            return val
        except ValueError:
            print(red("  Not a valid integer — try again"))

# ── Individual command handlers ────────────────────────────────────────────────

def cmd_show_char(ser):
    """m<ID>-<char>  — display a character."""
    mid = prompt_id()
    ch  = input(bold("  Character to display: ")).strip()
    if len(ch) != 1:
        print(red("  Enter exactly one character"))
        return
    idx = flap_index(ch)
    hint = f"  {dim(f'(flap index {idx})')}" if idx >= 0 else red("  ⚠ character not in flap set")
    print(hint)
    send(ser, build_message(mid, f"-{ch}"))

def cmd_show_index(ser):
    """m<ID>+<n>  — display a flap by index."""
    mid = prompt_id()
    idx = prompt_int("Flap index (0–63)", 0, 63)
    if idx is None:
        return
    print(dim(f"  (character at index {idx}: '{flap_char(idx)}')"))
    send(ser, build_message(mid, f"+{idx}"))

def cmd_home(ser):
    """m<ID>h  — home the module."""
    mid = prompt_id()
    send(ser, build_message(mid, "h"))

def cmd_calibrate(ser):
    """m<ID>c  — measure revolution length.  Response is printed by listener."""
    mid = prompt_id()
    print(dim("  (module will spin one full revolution and report step count)"))
    send(ser, build_message(mid, "c"))
    print(dim("  Waiting up to 15 s for calibration response…"))
    time.sleep(15)

def cmd_dump(ser):
    """m<ID>d  — dump EEPROM config.  Response printed by listener."""
    mid = prompt_id()
    print(dim("  (module will report home offset, total steps, and position map)"))
    send(ser, build_message(mid, "d"))
    time.sleep(1)

def cmd_set_home_offset(ser):
    """m<ID>o<n>  — set steps-from-Hall-to-zero."""
    mid = prompt_id()
    val = prompt_int("New home offset (steps)", 0)
    if val is None:
        return
    send(ser, build_message(mid, f"o{val}"))

def cmd_set_total_steps(ser):
    """m<ID>t<n>  — set total steps per revolution."""
    mid = prompt_id()
    val = prompt_int("Total steps per revolution", 1)
    if val is None:
        return
    send(ser, build_message(mid, f"t{val}"))

def cmd_nudge(ser):
    """m<ID>s<n>  — nudge N steps forward (accumulates into home offset)."""
    mid = prompt_id()
    val = prompt_int("Steps to nudge", 1)
    if val is None:
        return
    send(ser, build_message(mid, f"s{val}"))

def cmd_goto_step(ser):
    """m<ID>g<n>  — move to a raw step position."""
    mid = prompt_id()
    val = prompt_int("Target step position", 0)
    if val is None:
        return
    send(ser, build_message(mid, f"g{val}"))

def cmd_write_map(ser):
    """m<ID>w<idx>:<pos>  — write a calibrated step position into the EEPROM map."""
    mid = prompt_id()
    idx = prompt_int("Flap index (0–63)", 0, 63)
    if idx is None:
        return
    pos = prompt_int("Step position for this flap", 0)
    if pos is None:
        return
    print(dim(f"  (writing index {idx} '{flap_char(idx)}' = step {pos})"))
    send(ser, build_message(mid, f"w{idx}:{pos}"))

def cmd_set_id(ser):
    """m<ID>i<n>  — assign a new bus ID to a module."""
    print(yellow("  ⚠  This changes the module's address.  Use broadcast (*) to reach an unprovisioned module."))
    mid  = prompt_id()
    newid = prompt_int("New module ID (0–253)", 0, 253)
    if newid is None:
        return
    send(ser, build_message(mid, f"i{newid}"))

def cmd_auto_home(ser):
    """m<ID>a<0|1>  — enable or disable auto-home on boot."""
    mid = prompt_id()
    val = prompt_int("Auto-home on boot? (1=yes, 0=no)", 0, 1)
    if val is None:
        return
    send(ser, build_message(mid, f"a{val}"))

def cmd_erase_map(ser):
    """m<ID>e  — erase the entire EEPROM position map."""
    mid = prompt_id()
    confirm = input(bold(f"  Erase ALL calibrated positions for module {mid}? [y/N]: ")).strip().lower()
    if confirm == "y":
        send(ser, build_message(mid, "e"))
    else:
        print(dim("  Cancelled"))

def cmd_raw(ser):
    """Send a raw message string directly — useful for experimenting."""
    raw = input(bold("  Raw message (without leading 'm', e.g. '38-B'): ")).strip()
    if not raw:
        return
    send(ser, f"m{raw}")

def cmd_show_flap_table(_ser):
    """Print the full flap character table with indices."""
    print()
    print(bold("  Flap character table:"))
    print(dim("  idx  char    idx  char    idx  char    idx  char"))
    print(dim("  " + "─" * 50))
    for i in range(0, 64, 4):
        row = ""
        for j in range(4):
            k = i + j
            if k < len(FLAP_CHARS):
                ch = FLAP_CHARS[k]
                disp = repr(ch) if ch == " " else f" {ch} "
                row += f"  {k:>3}   {disp}   "
        print(row)
    print()

# ── Menu ───────────────────────────────────────────────────────────────────────

MENU = [
    ("Show character",           cmd_show_char),
    ("Show by flap index",       cmd_show_index),
    ("Home module",              cmd_home),
    ("Calibrate revolution",     cmd_calibrate),
    ("Dump EEPROM config",       cmd_dump),
    ("Set home offset",          cmd_set_home_offset),
    ("Set total steps/rev",      cmd_set_total_steps),
    ("Nudge steps (fine-tune home)", cmd_nudge),
    ("Go to raw step position",  cmd_goto_step),
    ("Write calibrated position to map", cmd_write_map),
    ("Set module ID",            cmd_set_id),
    ("Set auto-home on boot",    cmd_auto_home),
    ("Erase position map",       cmd_erase_map),
    ("Send raw message",         cmd_raw),
    ("Show flap character table",cmd_show_flap_table),
]

def print_menu():
    print()
    print(bold("─── Split-Flap RS-485 Test Tool ───────────────────"))
    for i, (label, _) in enumerate(MENU, 1):
        print(f"  {cyan(str(i)):>4}  {label}")
    print(f"  {cyan('q'):>4}  Quit")
    print()

def run(ser):
    start_listener(ser)
    while True:
        print_menu()
        try:
            choice = input(bold("command> ")).strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if choice in ("q", "quit", "exit"):
            break

        try:
            idx = int(choice) - 1
        except ValueError:
            print(red("  Enter a menu number or 'q' to quit"))
            continue

        if not (0 <= idx < len(MENU)):
            print(red(f"  Please enter a number between 1 and {len(MENU)}"))
            continue

        label, handler = MENU[idx]
        print(dim(f"\n  ── {label} ──"))
        try:
            handler(ser)
        except KeyboardInterrupt:
            print(dim("\n  (cancelled)"))

    stop_listener()
    ser.close()
    print(dim("Bye."))

# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Interactive RS-485 test tool for split-flap display modules"
    )
    parser.add_argument(
        "--port", default=None,
        help="Serial port (default: auto-detect USB adapter)"
    )
    parser.add_argument(
        "--baud", type=int, default=9600,
        help="Baud rate (default: 9600)"
    )
    args = parser.parse_args()

    port = args.port or choose_serial_port()
    ser = open_port(port, args.baud)
    run(ser)

if __name__ == "__main__":
    main()
