// ─────────────────────────────────────────────────────────────────────────────
// chord_detector.ino  — main sketch
//
// This file only does two things:
//   1. Owns the ChordEngine and OledDisplay instances
//   2. Drives a demo loop that feeds notes into the engine and renders results
//
// When MIDI is wired in, replace the demo_tick() call in loop() with
// calls to engine.noteOn() / engine.noteOff() from the USB MIDI callbacks.
// Nothing else in this file changes.
//
// Wokwi wiring (diagram.json):
//   GP4  → OLED SDA
//   GP5  → OLED SCL
//   3V3  → OLED VCC
//   GND  → OLED GND
// ─────────────────────────────────────────────────────────────────────────────

#include "ChordEngine.h"
#include "OledDisplay.h"

// ── Forward declarations ──────────────────────────────────────────────────────
void demo_advance();
void serial_print_chord();

// ── Global instances ──────────────────────────────────────────────────────────
ChordEngine   engine;
OledDisplay   oled;
ChordDisplayData displayData;

// ── Demo sequence ─────────────────────────────────────────────────────────────
// Each entry is a list of MIDI note numbers.
// 255 = end-of-chord sentinel.
static const uint8_t DEMO_NOTES[][8] = {
  { 60, 64, 67, 71, 255 },   // Cmaj7   C4 E4 G4 B4
  { 57, 60, 64, 67, 255 },   // Am7     A3 C4 E4 G4
  { 55, 59, 62, 65, 255 },   // G7      G3 B3 D4 F4
  { 59, 62, 65, 68, 255 },   // Bdim7   B3 D4 F4 Ab4
  { 53, 55, 60, 255, 0 },    // Fsus2   F3 G3 C4
  { 63, 67, 71, 255, 0 },    // Ebaug   Eb4 G4 B4
  { 60, 63, 67, 255, 0 },    // Cm      C4 Eb4 G4
  { 62, 65, 69, 72, 255 },   // Dm7     D4 F4 A4 C5
  { 255, 0, 0, 0, 0 },       // --- idle (no notes) ---
};
static const uint8_t NUM_DEMOS = sizeof(DEMO_NOTES) / sizeof(DEMO_NOTES[0]);

uint8_t  demoIdx    = 0;
uint32_t lastChange = 0;
const uint32_t HOLD_MS = 2800;    // how long each chord is held

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] MIDI Chord Detector starting..."));

  // Initialise display — will show splash then clear
  if (!oled.begin()) {
    Serial.println(F("[ERROR] OLED not found. Check wiring."));
    // Blink onboard LED as error indicator
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW);  delay(200);
    }
  }

  Serial.println(F("[OK] OLED ready"));

  // Initialise displayData to safe defaults
  memset(&displayData, 0, sizeof(displayData));
  displayData.dirty = true;

  lastChange = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Advance demo chord every HOLD_MS
  if (now - lastChange >= HOLD_MS) {
    demo_advance();
    lastChange = now;
  }

  // Ask engine to fill display data (only marks dirty if chord changed)
  engine.fillDisplayData(displayData);

  // Render to OLED (only redraws if dirty)
  oled.update(displayData);

  // Print to serial for debugging
  if (displayData.dirty) {
    serial_print_chord();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// demo_advance — loads the next chord from the demo table into the engine
// ─────────────────────────────────────────────────────────────────────────────
void demo_advance() {
  engine.allNotesOff();

  demoIdx = (demoIdx + 1) % NUM_DEMOS;

  const uint8_t* notes = DEMO_NOTES[demoIdx];
  for (uint8_t i = 0; i < 8; i++) {
    if (notes[i] == 255) break;
    engine.noteOn(notes[i]);
  }

  Serial.print(F("[DEMO] chord "));
  Serial.print(demoIdx);
  Serial.print(F(" — notes: "));
  char buf[32];
  engine.activeNotesString(buf, sizeof(buf));
  Serial.println(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// serial_print_chord — prints current detection result to Serial
// ─────────────────────────────────────────────────────────────────────────────
void serial_print_chord() {
  if (!displayData.has_chord) {
    Serial.println(F("[CHORD] --- (no notes)"));
    return;
  }
  Serial.print(F("[CHORD] "));
  Serial.print(displayData.symbol);
  Serial.print(F("  |  "));
  Serial.print(displayData.full_name);
  Serial.print(F("  |  "));
  Serial.print(displayData.inversion);
  Serial.print(F("  |  notes: "));
  Serial.print(displayData.notes_str);
  Serial.print(F("  |  scales: "));
  for (uint8_t i = 0; i < displayData.num_scales; i++) {
    Serial.print(displayData.scales[i]);
    if (i < displayData.num_scales - 1) Serial.print(F(", "));
  }
  Serial.println();
}
