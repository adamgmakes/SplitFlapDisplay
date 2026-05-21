
// ==============================================================================
// Split-Flap Display Module Firmware — v7
// ==============================================================================
// Each Arduino runs this firmware to control one character cell of a split-flap
// display. The cell is driven by a 28BYJ-48 stepper motor (or similar) which
// rotates a reel of physical flaps past a viewing window. A Hall effect sensor
// detects a magnet on the reel to find the "home" (zero) position.
//
// Multiple modules are wired together on an RS-485 serial bus and addressed by
// a unique numeric ID. A Raspberry Pi (or other controller) sends commands over
// the bus; each module listens for its own ID and responds accordingly.
//
// Message format:  m<ID><CMD>[data]\n
//   <ID> is 1–3 digits (e.g. 5, 38, 138) or one or more '*' for broadcast.
//   Examples:
//     "m38-B"    →  module 38: show character 'B'     ('-' always takes one char)
//     "m38+7"    →  module 38: show flap at index 7   ('+' takes a numeric index)
//     "m38+12"   →  module 38: show flap at index 12
//     "m138-A"   →  module 138: show character 'A'    (3-digit ID)
//     "m*h"      →  broadcast: home all modules
//     "m**h"     →  broadcast: home all modules (legacy two-star format still works)
//
// Change log v6 → v7:
//   - Module IDs now 1–3 digits; ID parsing is digit-accumulation based, not
//     fixed-width, so 'm5', 'm38', and 'm138' all work correctly.
//   - New '+' command: move to a flap by numeric index (0–63). The existing '-'
//     command is unchanged — it still reads exactly one character byte.
//   - moveToChar() refactored to call the new moveToIndex() core function.
// ==============================================================================

#include <SoftwareSerial.h>   // Software UART for RS-485 (frees hardware serial)
#include <EEPROM.h>           // Non-volatile storage for calibration & config

// ==========================================
//               CONFIGURATION
// ==========================================
// !!! CHANGE THIS FOR EACH MODULE TODAY !!!
// This is burned into EEPROM on first boot if no saved ID exists.
const uint8_t HARDCODED_ID = 01;

// The ordered set of 64 characters this reel can display.
// Position 0 = blank space (the "home" flap).
// The index in this string corresponds to a physical flap on the reel.
// Lowercase letters at the end (q, r, o, y, g, b, p, w) represent color flaps.
const String FLAP_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$&()-+=;q:%'.,/?*roygbpw";

// ---- EEPROM Address Map ----
// Each address stores a specific configuration value persistently.
const int ADDR_INIT        = 0;   // 1 byte  — Magic number to detect valid EEPROM data
const int ADDR_HOME_OFFSET = 1;   // 2 bytes — Steps past magnet trigger to reach flap 0
const int ADDR_TOTAL_STEPS = 3;   // 2 bytes — Total steps for one full reel revolution
const int ADDR_MODULE_ID   = 5;   // 1 byte  — This module's bus ID (0–254; 255 = unset)
const int ADDR_AUTO_HOME   = 6;   // 1 byte  — 1 = home on boot, 0 = restore saved position
const int ADDR_SAVED_POS   = 7;   // 2 bytes — Last known step position (for no-home mode)
const int ADDR_SAVED_INDEX = 9;   // 1 byte  — Last known flap index  (for no-home mode)
const int ADDR_MAP_START   = 12;  // 128 bytes — Fine-calibrated step positions for all 64 flaps
                                  //             Each entry is 2 bytes (uint16_t), 0xFFFF = uncalibrated

// Magic value written to ADDR_INIT to indicate EEPROM has been initialized.
// Changing this value forces all modules to reset to defaults on next boot.
const uint8_t INIT_VALUE = 0x5D;

// ---- Operating State ----
uint8_t moduleId        = 255;  // 255 = unassigned; loaded from EEPROM on boot
bool    autoHomeEnabled = true; // Whether to run homeModule() at startup

// Calibration values (loaded from EEPROM; can be updated via serial commands)
int stepsFromHallToZero = 2832;  // Steps from Hall sensor trigger edge → flap 0 position
int totalStepsPerRev    = 4096;  // Total half-steps in one full reel revolution

// ==========================================
//              PIN DEFINITIONS
// ==========================================
// RS-485 transceiver pins (half-duplex; DE pin selects TX vs RX mode)
const int RS485_RX = 3;   // Receive data from bus (SoftwareSerial RX)
const int RS485_TX = 1;   // Transmit data to bus  (SoftwareSerial TX)
const int RS485_DE = 2;   // Driver Enable: HIGH = transmit, LOW = receive

// Stepper motor coil drive pins (connected to ULN2003 or similar driver board)
#define IN1 9
#define IN2 8
#define IN3 7
#define IN4 6

// Hall effect sensor (active LOW — pulls LOW when magnet is present)
#define HALL_PIN 4

// ==========================================
//               GLOBALS
// ==========================================
// SoftwareSerial lets us use arbitrary pins for RS-485, keeping hardware serial
// free for debugging if needed.
SoftwareSerial rs485(RS485_RX, RS485_TX);

long currentStepPos   = 0;   // Current motor position in half-steps (0 = flap 0)
int  currentPhase     = 0;   // Current index into halfStepSequence[8]
int  parseState       = 0;   // State machine position for incoming serial parsing
int  currentFlapIndex = -1;  // Which flap is currently showing (-1 = unknown)
int  tempIndex        = -1;  // Scratch variable used while parsing 'w' (write map) command

const int stepDelay = 1;     // Milliseconds between each half-step pulse (controls speed)

String buffer     = "";      // Accumulates digit characters during command parsing
String idBuffer   = "";      // Accumulates digit characters of the incoming module ID
bool   idWildcard = false;   // True when '*' was seen in the ID field (broadcast match)

unsigned long lastSerialTime = 0;  // Timestamp of last received byte (for timeout detection)

// Half-step sequence for 4-wire stepper motor.
// Each row is one phase: {IN1, IN2, IN3, IN4} — energize the coils in this order
// to step forward. Reversing the traversal order steps backward.
// Half-stepping (8 phases vs 4) gives smoother motion and finer resolution.
const uint8_t halfStepSequence[8][4] = {
  {1, 0, 0, 0},   // Phase 0: coil A only
  {1, 1, 0, 0},   // Phase 1: coils A+B
  {0, 1, 0, 0},   // Phase 2: coil B only
  {0, 1, 1, 0},   // Phase 3: coils B+C
  {0, 0, 1, 0},   // Phase 4: coil C only
  {0, 0, 1, 1},   // Phase 5: coils C+D
  {0, 0, 0, 1},   // Phase 6: coil D only
  {1, 0, 0, 1}    // Phase 7: coils D+A
};

// ==========================================
//             EEPROM FUNCTIONS
// ==========================================

// Persist the home offset (steps from Hall trigger to flap 0) to EEPROM.
void saveHomeOffset() { EEPROM.put(ADDR_HOME_OFFSET, stepsFromHallToZero); }

// Persist the total steps per revolution to EEPROM.
void saveTotalSteps()  { EEPROM.put(ADDR_TOTAL_STEPS, totalStepsPerRev); }

// Save current position and flap index so the module can resume without homing.
// Only saves if autoHome is disabled — if we're going to home on boot anyway,
// there's no point wasting EEPROM write cycles.
void saveState() {
  if (!autoHomeEnabled) {
    EEPROM.put(ADDR_SAVED_POS,   currentStepPos);
    EEPROM.put(ADDR_SAVED_INDEX, currentFlapIndex);
  }
}

// Print the module ID onto the RS-485 bus, zero-padded to at least 2 digits.
// Examples: 5 → "05",  38 → "38",  138 → "138"
void printModuleId() {
  if (moduleId < 10) rs485.print("0");
  rs485.print(moduleId);
}

// Send a full EEPROM config dump back to the Pi over RS-485.
// Output format:  m<ID>d:<homeOffset>:<totalSteps>:<idx>=<pos>,<idx>=<pos>,...\n
// Only non-empty map entries (pos != 0xFFFF) are included.
void dumpEeprom() {
  // Wait for the RS-485 USB dongle on the Pi side to switch into receive mode.
  delay(50);
  digitalWrite(RS485_DE, HIGH); // Enable transmit
  delay(10);

  rs485.print("m");
  printModuleId();
  rs485.print("d:");
  rs485.print(stepsFromHallToZero);
  rs485.print(":");
  rs485.print(totalStepsPerRev);
  rs485.print(":");

  // Print all calibrated flap positions from the EEPROM map
  bool first = true;
  for (int i = 0; i < 64; i++) {
    uint16_t pos = 0xFFFF;
    EEPROM.get(ADDR_MAP_START + (i * 2), pos);
    if (pos != 0xFFFF) {          // 0xFFFF means "not calibrated — skip"
      if (!first) rs485.print(",");
      rs485.print(i);
      rs485.print("=");
      rs485.print(pos);
      first = false;
    }
  }

  rs485.print("\n");
  delay(100);                    // Let the transmission complete before releasing the bus
  digitalWrite(RS485_DE, LOW);  // Switch back to receive mode
}

// ==========================================
//             MOTOR FUNCTIONS
// ==========================================

// Drive the four stepper coil pins to match the given phase pattern.
// `step` is a pointer to one row of halfStepSequence.
void applyStep(const uint8_t *step) {
  digitalWrite(IN1, step[0]);
  digitalWrite(IN2, step[1]);
  digitalWrite(IN3, step[2]);
  digitalWrite(IN4, step[3]);
}

// Advance the reel by `steps` half-steps in the "backward" phase direction.
// "Backward" here means decrementing the phase index, which moves the motor
// in the direction that advances flaps (forward on the display = backward in phase).
//
// Also tracks the Hall sensor edge: when the magnet is first detected, the
// current step position is corrected using the known offset, giving us an
// absolute position fix mid-movement.
void stepBackward(int steps) {
  static bool lastHallState = false; // Persists across calls to detect rising edge

  for (int k = 0; k < steps; k++) {
    bool hallNow = hallActive();

    // Rising edge of Hall sensor = magnet just arrived under the sensor.
    // Use this as an absolute position anchor.
    if (hallNow && !lastHallState) {
      // At this moment, we're (totalStepsPerRev - stepsFromHallToZero) steps before
      // completing a full revolution (i.e., returning to flap 0).
      currentStepPos = totalStepsPerRev - stepsFromHallToZero;
    }
    lastHallState = hallNow;

    // Step the motor backward in phase (decrement, wrap around)
    currentPhase--;
    if (currentPhase < 0) currentPhase = 7;

    applyStep(halfStepSequence[currentPhase]);
    delay(stepDelay);

    // Track absolute position, wrapping at the revolution boundary
    currentStepPos++;
    if (currentStepPos >= totalStepsPerRev) currentStepPos = 0;
  }
}

// De-energize all stepper coils to prevent heat buildup when idle.
// The motor holds its last position magnetically even with coils off
// (at least well enough for display purposes).
void releaseMotor() {
  digitalWrite(IN1, 0);
  digitalWrite(IN2, 0);
  digitalWrite(IN3, 0);
  digitalWrite(IN4, 0);
}

// Returns true if the Hall effect sensor detects the magnet (active LOW).
bool hallActive() {
  return (digitalRead(HALL_PIN) == LOW);
}

// ==========================================
//             LOGIC FUNCTIONS
// ==========================================

// Drive the reel to the physical zero position (flap 0 = blank space).
// Steps:
//   1. Spin until the Hall sensor fires (magnet found), with a safety limit.
//   2. Continue spinning stepsFromHallToZero more steps to land on flap 0.
//   3. Reset position tracking and release the motor.
void homeModule() {
  long safety = 0;

  // Step until the Hall sensor triggers, or bail after slightly more than one
  // full revolution (prevents infinite loops if sensor is broken/missing).
  while (!hallActive() && safety < (totalStepsPerRev + 500)) {
    stepBackward(1);
    safety++;
  }

  // Advance the calibrated offset to reach flap 0
  stepBackward(stepsFromHallToZero);

  // We're now at flap 0 — reset tracking
  currentStepPos   = 0;
  currentFlapIndex = 0;
  releaseMotor();
}

// Measure the actual number of steps in one full revolution by finding the
// Hall sensor twice and counting steps between them. Reports the result to
// the Pi and then re-homes.
//
// This is run once during physical installation or after a reel swap.
void calibrateModule() {
  long safety = 0;

  // Phase 1: If already on the magnet, move off it first
  while (hallActive() && safety < 4000) {
    stepBackward(1);
    safety++;
    delay(5);
  }

  // Phase 2: Find the leading edge of the magnet
  safety = 0;
  while (!hallActive() && safety < 5000) {
    stepBackward(1);
    safety++;
  }

  // Phase 3: Find the trailing edge of the magnet (so we start counting from a clean edge)
  while (hallActive()) {
    stepBackward(1);
  }

  // Phase 4: Count steps for one full revolution (trailing edge → next trailing edge)
  int measuredSteps = 0;
  while (!hallActive() && measuredSteps < 5000) {
    stepBackward(1);
    measuredSteps++;
  }
  while (hallActive()) {
    stepBackward(1);
    measuredSteps++;
  }

  // Report measured step count to Pi over RS-485
  delay(50);
  digitalWrite(RS485_DE, HIGH);
  delay(10);

  rs485.print("m");
  printModuleId();
  rs485.print(":");
  rs485.print(measuredSteps);
  rs485.print("\n");

  delay(100);
  digitalWrite(RS485_DE, LOW);

  // Save the new measurement and return to home position
  totalStepsPerRev = measuredSteps;
  saveTotalSteps();
  homeModule();
  saveState();
}

// Move the reel to display the flap at the given index (0–63).
//
// Resolution strategy:
//   1. Look up the fine-calibrated position for this flap index in EEPROM.
//   2. If not calibrated yet, estimate using even division of the revolution.
//
// Movement is always forward (same direction as homeModule) to avoid backlash.
// If we've overshot the target, we'll go almost a full revolution forward.
void moveToIndex(int targetIndex) {
  // Validate the index is within the reel's character set
  if (targetIndex < 0 || targetIndex >= (int)FLAP_CHARS.length()) return;

  // Already showing the right flap — nothing to do
  if (currentFlapIndex == targetIndex) return;

  // If position is unknown, home first to get a reliable starting point
  if (currentFlapIndex == -1) {
    homeModule();
  }

  // Try to get a fine-calibrated position from EEPROM
  uint16_t mappedPos = 0xFFFF;
  EEPROM.get(ADDR_MAP_START + (targetIndex * 2), mappedPos);

  long targetStepPos;
  if (mappedPos != 0xFFFF) {
    // Use the precise calibrated position
    targetStepPos = mappedPos;
  } else {
    // Fall back to evenly spacing 64 flaps across the revolution
    targetStepPos = ((long)targetIndex * (long)totalStepsPerRev) / 64;
  }

  // Calculate forward distance to the target (always positive — reel only goes one way)
  long stepsToMove = targetStepPos - currentStepPos;
  if (stepsToMove < 0) {
    stepsToMove += totalStepsPerRev; // Wrap: go the long way around
  }

  // Drive the motor
  while (stepsToMove > 0) {
    stepBackward(1);
    stepsToMove--;
  }

  releaseMotor();
  currentFlapIndex = targetIndex;
  saveState(); // Persist position in case of power loss
}

// Move the reel to display a specific character.
// Looks up the character's index in FLAP_CHARS and delegates to moveToIndex().
// Returns without moving if the character is not on this reel.
void moveToChar(char targetChar) {
  int targetIndex = FLAP_CHARS.indexOf(targetChar);
  if (targetIndex == -1) return; // Character not on this reel
  moveToIndex(targetIndex);
}

// ==========================================
//              SETUP
// ==========================================
void setup() {
  // Configure stepper motor output pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Hall sensor: active LOW with internal pull-up so it reads HIGH when no magnet
  pinMode(HALL_PIN, INPUT_PULLUP);

  // RS-485 direction control: start in receive mode
  pinMode(RS485_DE, OUTPUT);
  digitalWrite(RS485_DE, LOW);

  // ---- Load config from EEPROM ----
  if (EEPROM.read(ADDR_INIT) == INIT_VALUE) {
    // EEPROM has been initialized before — restore saved values
    EEPROM.get(ADDR_HOME_OFFSET, stepsFromHallToZero);
    EEPROM.get(ADDR_TOTAL_STEPS, totalStepsPerRev);
    moduleId        = EEPROM.read(ADDR_MODULE_ID);
    autoHomeEnabled = (EEPROM.read(ADDR_AUTO_HOME) == 1);
  } else {
    // First boot — write defaults to EEPROM
    EEPROM.write(ADDR_INIT, INIT_VALUE);
    saveHomeOffset();
    saveTotalSteps();

    moduleId = HARDCODED_ID;
    EEPROM.write(ADDR_MODULE_ID, moduleId);

    EEPROM.write(ADDR_AUTO_HOME, 1); // Default: home on every boot
    autoHomeEnabled = true;

    // Initialize the entire position map to 0xFFFF (uncalibrated)
    for (int i = 0; i < 64; i++) {
      uint16_t empty = 0xFFFF;
      EEPROM.put(ADDR_MAP_START + (i * 2), empty);
    }
  }

  rs485.begin(9600);

  // ---- Staggered startup ----
  // Delay proportional to module ID before homing so all motors in a large
  // display don't surge current at the same instant (e.g. module 38 waits 5.7s).
  if (moduleId != 255) {
    delay(moduleId * 150);
  }

  // ---- Home or restore position ----
  if (autoHomeEnabled) {
    // Spin to the Hall sensor and advance to flap 0
    homeModule();
    saveState();
  } else {
    // Restore last known position without moving (trust the saved values)
    EEPROM.get(ADDR_SAVED_POS, currentStepPos);
    if (currentStepPos >= totalStepsPerRev) currentStepPos = 0; // Sanity check
    currentFlapIndex = (int8_t)EEPROM.read(ADDR_SAVED_INDEX);
  }
}

// ==========================================
//              MAIN LOOP
// ==========================================
// The main loop implements a state machine that parses incoming RS-485 messages
// one character at a time. A valid message has this structure:
//
//   m  <ID digits or *>  <CMD>  [data]  \n
//   │        │             │      │
//   │        │             │      └─ Command-specific payload (digits, a char, etc.)
//   │        │             └──────── Command letter (see state 1 dispatch below)
//   │        └────────────────────── 1–3 digit module ID, or '*' (one or more) to broadcast
//   └──────────────────────────────── Literal 'm' — start-of-message marker
//
// parseState values:
//   0       = idle, waiting for 'm'
//   1       = reading ID field (digits or '*'), dispatches command on first non-digit
//   4       = '-' command: reading the single display character byte
//   5       = 'o' command: accumulating home-offset digits
//   6       = 't' command: accumulating total-steps digits
//   7       = 's' command: accumulating nudge-steps digits
//   8       = 'g' command: accumulating raw step-position digits
//   9       = 'w' command: accumulating index then ':' then position digits
//   10      = 'i' command: accumulating new module-ID digits
//   11      = 'a' command: accumulating auto-home toggle digit
//   12      = '+' command: accumulating numeric flap-index digits
//
void loop() {
  // ---- Process incoming serial bytes ----
  while (rs485.available()) {
    char c = rs485.read();
    lastSerialTime = millis(); // Track time of most recent byte for timeout logic

    switch (parseState) {

      // State 0: Idle — watch for 'm' start-of-message marker.
      case 0:
        if (c == 'm') {
          idBuffer   = "";    // Clear ID accumulator for the new message
          idWildcard = false;
          parseState = 1;
        }
        break;

      // State 1: Accumulate the module ID field, then dispatch the command.
      //
      // While receiving digits or '*', we stay in state 1.
      // The first character that is neither a digit nor '*' is the command letter.
      // At that point we check whether the accumulated ID matches this module,
      // then immediately dispatch the command (same byte, no extra state needed).
      //
      // Supported ID formats:
      //   "m5cmd"    — 1-digit ID
      //   "m38cmd"   — 2-digit ID  (original format)
      //   "m138cmd"  — 3-digit ID
      //   "m*cmd"    — wildcard broadcast (single star)
      //   "m**cmd"   — wildcard broadcast (legacy two-star format)
      case 1:
        if (c == '*') {
          idWildcard = true;  // Any further digits are ignored; this is now a broadcast
        } else if (isDigit(c)) {
          idBuffer += c;      // Accumulate another digit of the module ID
        } else {
          // c is the command character. Check if this message is addressed to us.
          bool match = idWildcard ||
                       (idBuffer.length() > 0 && (uint8_t)idBuffer.toInt() == moduleId);

          if (match) {
            // Dispatch command — same logic as old "case 3", now inlined here.
            if      (c == '-') { parseState = 4; }                            // Display character (one byte)
            else if (c == '+') { buffer = "";   parseState = 12; }            // Display by index (digits)
            else if (c == 'h') { homeModule(); saveState(); parseState = 0; } // Home
            else if (c == 'c') { calibrateModule();          parseState = 0; } // Calibrate revolution
            else if (c == 'o') { buffer = "";                parseState = 5; } // Set home offset
            else if (c == 't') { buffer = "";                parseState = 6; } // Set total steps
            else if (c == 's') { buffer = "";                parseState = 7; } // Nudge N steps
            else if (c == 'g') { buffer = "";                parseState = 8; } // Go to raw step
            else if (c == 'w') { buffer = ""; tempIndex = -1; parseState = 9; }// Write map entry
            else if (c == 'i') { buffer = "";                parseState = 10;} // Set module ID
            else if (c == 'a') { buffer = "";                parseState = 11;} // Set auto-home
            else if (c == 'd') { dumpEeprom();               parseState = 0; } // Dump EEPROM
            else if (c == 'e') {                                               // Erase position map
              for (int i = 0; i < 64; i++) {
                uint16_t empty = 0xFFFF;
                EEPROM.put(ADDR_MAP_START + (i * 2), empty);
              }
              parseState = 0;
            }
            else parseState = 0; // Unknown command — ignore
          } else {
            parseState = 0; // Not our ID — ignore and return to idle
          }

          // Always reset the ID accumulators whether or not we matched
          idBuffer   = "";
          idWildcard = false;
        }
        break;

      // State 4: '-' command — the next single byte is the target display character.
      // Any printable character in FLAP_CHARS is valid, including digits.
      // e.g. "m38-B" shows 'B', "m38-5" shows the character '5' (not index 5).
      case 4:
        moveToChar(c);
        parseState = 0;
        break;

      // State 5: 'o' command — read digits for new home offset value.
      // Terminated by any non-digit (typically '\n').
      case 5:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            stepsFromHallToZero = buffer.toInt();
            saveHomeOffset();
          }
          parseState = 0;
        }
        break;

      // State 6: 't' command — read digits for new total-steps-per-revolution value.
      case 6:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            totalStepsPerRev = buffer.toInt();
            saveTotalSteps();
          }
          parseState = 0;
        }
        break;

      // State 7: 's' command — nudge the reel by N steps and accumulate into home offset.
      // Used for fine-tuning during physical alignment without a full recalibration.
      case 7:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            int stepsToMove = buffer.toInt();
            stepBackward(stepsToMove);
            releaseMotor();
            stepsFromHallToZero += stepsToMove; // Grow the offset so future homes land here
            saveHomeOffset();
          }
          parseState = 0;
        }
        break;

      // State 8: 'g' command — move to an absolute raw step position.
      // Sets currentFlapIndex to -2 (unknown/manual) since we're bypassing character logic.
      case 8:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            long targetStep  = buffer.toInt();
            long stepsToMove = targetStep - currentStepPos;
            if (stepsToMove < 0) stepsToMove += totalStepsPerRev; // Wrap
            while (stepsToMove > 0) { stepBackward(1); stepsToMove--; }
            releaseMotor();
            currentFlapIndex = -2; // Position known in steps but not as a named character
            saveState();
          }
          parseState = 0;
        }
        break;

      // State 9: 'w' command — write a calibrated position into the EEPROM map.
      // Format: w<index>:<stepPosition>
      // Parsing: accumulate index digits, ':' splits index from position, then position digits.
      case 9:
        if (c == ':') {
          // Colon separator: buffer holds the flap index, now read the step position
          tempIndex = buffer.toInt();
          buffer    = "";
        } else if (isDigit(c)) {
          buffer += c;
        } else {
          // Terminator: commit the position to EEPROM
          if (buffer.length() > 0 && tempIndex != -1) {
            uint16_t pos = buffer.toInt();
            EEPROM.put(ADDR_MAP_START + (tempIndex * 2), pos);
          }
          parseState = 0;
        }
        break;

      // State 10: 'i' command — assign a new module ID and persist it.
      case 10:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            moduleId = buffer.toInt();
            EEPROM.write(ADDR_MODULE_ID, moduleId);
            // No idChars to update — ID matching now uses digit accumulation in state 1
          }
          parseState = 0;
        }
        break;

      // State 11: 'a' command — enable (1) or disable (0) auto-home on boot.
      case 11:
        if (isDigit(c)) {
          buffer += c;
        } else {
          if (buffer.length() > 0) {
            autoHomeEnabled = (buffer.toInt() == 1);
            EEPROM.write(ADDR_AUTO_HOME, autoHomeEnabled ? 1 : 0);
            saveState(); // If disabling auto-home, save current position immediately
          }
          parseState = 0;
        }
        break;

      // State 12: '+' command — accumulate numeric index digits, then move to that flap.
      // e.g. "m38+7\n" → index 7,  "m38+12\n" → index 12
      // Terminated by any non-digit (typically '\n').
      // On terminator, call moveToIndex() with the fully accumulated number.
      case 12:
        if (isDigit(c)) {
          buffer += c;
        } else {
          moveToIndex(buffer.toInt());
          parseState = 0;
        }
        break;
    }
  }

  // ---- Timeout: flush incomplete numeric commands ----
  // If we're in a data-reading state (5–12) and no byte has arrived in 50ms,
  // treat whatever is in the buffer as the complete value and execute it.
  // This handles messages that don't have an explicit terminator or whose
  // terminator was missed (e.g. the Pi sent digits then dropped the connection).
  if ((parseState >= 5 && parseState <= 12) && (millis() - lastSerialTime > 50)) {
    if (buffer.length() > 0) {
      if (parseState == 5) {
        stepsFromHallToZero = buffer.toInt();
        saveHomeOffset();
      }
      if (parseState == 6) {
        totalStepsPerRev = buffer.toInt();
        saveTotalSteps();
      }
      if (parseState == 7) {
        int stepsToMove = buffer.toInt();
        stepBackward(stepsToMove);
        releaseMotor();
        stepsFromHallToZero += stepsToMove;
        saveHomeOffset();
      }
      if (parseState == 8) {
        long targetStep  = buffer.toInt();
        long stepsToMove = targetStep - currentStepPos;
        if (stepsToMove < 0) stepsToMove += totalStepsPerRev;
        while (stepsToMove > 0) { stepBackward(1); stepsToMove--; }
        releaseMotor();
        currentFlapIndex = -2;
        saveState();
      }
      if (parseState == 9 && tempIndex != -1) {
        uint16_t pos = buffer.toInt();
        EEPROM.put(ADDR_MAP_START + (tempIndex * 2), pos);
      }
      if (parseState == 10) {
        moduleId = buffer.toInt();
        EEPROM.write(ADDR_MODULE_ID, moduleId);
      }
      if (parseState == 11) {
        autoHomeEnabled = (buffer.toInt() == 1);
        EEPROM.write(ADDR_AUTO_HOME, autoHomeEnabled ? 1 : 0);
        saveState();
      }
      if (parseState == 12) {
        // Timed-out index value from a '+' command (no terminator received)
        moveToIndex(buffer.toInt());
      }
    }
    parseState = 0; // Return to idle regardless
  }
}
