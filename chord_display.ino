// ─────────────────────────────────────────────────────────────────────────────
// MIDI Chord Detector — OLED Display Module
// Target : Raspberry Pi Pico (RP2040) via arduino-pico core
// Simulator : Wokwi  (paste all three files: .ino + diagram.json + libraries.txt)
//
// I2C wiring:
//   SDA → GP4  (Pico pin 6)
//   SCL → GP5  (Pico pin 7)
//   VCC → 3.3V
//   GND → GND
//
// Display layout (128×64 pixels):
//   ┌──────────────────────────────┐
//   │ Cmaj7/E                      │  y=0..18  big chord symbol  (size 3)
//   ├──────────────────────────────┤
//   │ C Major 7th · 1st inv.       │  y=20..28 full name + inversion (size 1)
//   ├──────────────────────────────┤
//   │ ♩ C4  E4  G4  B4            │  y=30..38 active notes (size 1)
//   ├──────────────────────────────┤
//   │ Ionian  Lydian               │  y=41..49 scale row 1 (size 1)
//   │ Mixolyd  MajPent             │  y=52..60 scale row 2 (size 1)
//   └──────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Hardware ─────────────────────────────────────────────────────────────────
#define SCREEN_W    128
#define SCREEN_H    64
#define OLED_ADDR   0x3C
#define OLED_RESET  -1
#define I2C_SDA     4
#define I2C_SCL     5

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// ── Data structures ──────────────────────────────────────────────────────────
struct ChordResult {
  const char* symbol;        // "Cmaj7/E"
  const char* full_name;     // "C Major 7th"
  const char* inversion;     // "1st inv."
  const char* notes;         // "C4 E4 G4 B4"
  const char* scale1a;       // first scale, column 1
  const char* scale1b;       // first scale, column 2
  const char* scale2a;       // second scale, column 1
  const char* scale2b;       // second scale, column 2
  bool        active;        // false = show idle screen
};

// ── Sample chords for the demo cycle ─────────────────────────────────────────
// These will rotate every 3 seconds to show the full display range.
// Replace with live ChordEngine output when MIDI is wired in.
static const ChordResult DEMO_CHORDS[] = {
  {
    "Cmaj7/E",
    "C Major 7th",
    "1st inv.",
    "C4  E4  G4  B4",
    "C Ionian",   "G Mixolyd",
    "F Lydian",   "D Dorian",
    true
  },
  {
    "Am7",
    "A Minor 7th",
    "Root pos.",
    "A3  C4  E4  G4",
    "C Ionian",   "A Aeolian",
    "G Mixolyd",  "E Phrygian",
    true
  },
  {
    "G7",
    "G Dominant 7th",
    "Root pos.",
    "G3  B3  D4  F4",
    "C Ionian",   "G Mixolyd",
    "F Lydian",   "Bb Ionn",
    true
  },
  {
    "Bdim7",
    "B Diminished 7th",
    "Root pos.",
    "B3  D4  F4  Ab4",
    "C HarmMin",  "E HarmMin",
    "G HarmMin",  "Bb HarmMin",
    true
  },
  {
    "Fsus2",
    "F Suspended 2nd",
    "Root pos.",
    "F3  G3  C4",
    "C Ionian",   "F Ionian",
    "Bb Ionian",  "G Mixolyd",
    true
  },
  {
    "Ebaug",
    "Eb Augmented",
    "Root pos.",
    "Eb4  G4  B4",
    "Eb WholeTn", "C WholeTn",
    "AugMaj7 rel","",
    true
  },
  {
    // idle — no notes held
    "---",
    "",
    "",
    "",
    "", "",
    "", "",
    false
  }
};

static const int NUM_DEMOS = sizeof(DEMO_CHORDS) / sizeof(DEMO_CHORDS[0]);

// ── State ─────────────────────────────────────────────────────────────────────
int  demoIndex    = 0;
bool needsRedraw  = true;
uint32_t lastSwap = 0;
const uint32_t SWAP_MS = 3000;   // rotate demo every 3 s

// ── Forward declarations ──────────────────────────────────────────────────────
void renderChord(const ChordResult& c);
void renderIdle();
void drawDivider(int y);

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERROR] SSD1306 init failed"));
    while (true) { delay(500); }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ── Boot splash ───────────────────────────────────────────────────────────
  display.setTextSize(1);
  display.setCursor(18, 20);
  display.println(F("MIDI Chord Detector"));
  display.setCursor(34, 34);
  display.println(F("RP2040  v0.1.0"));
  display.display();
  delay(1800);

  lastSwap = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Advance demo every SWAP_MS
  if (now - lastSwap >= SWAP_MS) {
    demoIndex  = (demoIndex + 1) % NUM_DEMOS;
    lastSwap   = now;
    needsRedraw = true;
  }

  if (needsRedraw) {
    needsRedraw = false;
    const ChordResult& c = DEMO_CHORDS[demoIndex];
    if (c.active) {
      renderChord(c);
    } else {
      renderIdle();
    }
    display.display();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// renderChord — draws the full 5-zone layout for an active chord
// ─────────────────────────────────────────────────────────────────────────────
void renderChord(const ChordResult& c) {
  display.clearDisplay();

  // ── Zone 1: chord symbol (big) ────────────────────────────────────────────
  // Size 3 = 18px tall glyphs. Fits ~7 chars at that size in 128px.
  // For longer symbols we fall back to size 2.
  int symLen = strlen(c.symbol);
  if (symLen <= 6) {
    display.setTextSize(3);
    display.setCursor(0, 0);
  } else {
    display.setTextSize(2);
    display.setCursor(0, 2);
  }
  display.print(c.symbol);

  // ── Zone 2: full name + inversion ────────────────────────────────────────
  drawDivider(19);
  display.setTextSize(1);
  display.setCursor(0, 21);

  // Truncate full_name + inversion to fit 128px (21 chars × 6px = 126)
  char nameLine[22];
  if (strlen(c.inversion) > 0) {
    snprintf(nameLine, sizeof(nameLine), "%-13s%s", c.full_name, c.inversion);
  } else {
    snprintf(nameLine, sizeof(nameLine), "%s", c.full_name);
  }
  display.print(nameLine);

  // ── Zone 3: active notes ──────────────────────────────────────────────────
  drawDivider(30);
  display.setTextSize(1);
  display.setCursor(0, 32);
  if (strlen(c.notes) > 0) {
    // small note symbol via a simple dash prefix
    display.print(F("  "));
    display.print(c.notes);
  }

  // ── Zone 4 & 5: scale context ────────────────────────────────────────────
  drawDivider(41);
  display.setTextSize(1);

  // Row 1 — two columns of 10 chars each
  display.setCursor(0, 43);
  if (strlen(c.scale1a) > 0) {
    char row[22];
    snprintf(row, sizeof(row), "%-14s%s", c.scale1a, c.scale1b);
    display.print(row);
  }

  // Row 2
  display.setCursor(0, 53);
  if (strlen(c.scale2a) > 0) {
    char row[22];
    snprintf(row, sizeof(row), "%-14s%s", c.scale2a, c.scale2b);
    display.print(row);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// renderIdle — shown when no notes are held
// ─────────────────────────────────────────────────────────────────────────────
void renderIdle() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(22, 10);
  display.print(F("Play a chord..."));

  // draw a small static keyboard hint
  // white keys
  for (int k = 0; k < 7; k++) {
    display.drawRect(20 + k * 13, 30, 12, 24, SSD1306_WHITE);
  }
  // black keys (gaps between C#,D#, F#,G#,A#)
  int blackOffsets[] = {7, 20, 46, 59, 72};
  for (int i = 0; i < 5; i++) {
    display.fillRect(20 + blackOffsets[i], 30, 8, 14, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setCursor(30, 58);
  display.print(F("MIDI Chord Detector"));
}

// ─────────────────────────────────────────────────────────────────────────────
// drawDivider — 1px horizontal rule
// ─────────────────────────────────────────────────────────────────────────────
void drawDivider(int y) {
  display.drawFastHLine(0, y, SCREEN_W, SSD1306_WHITE);
}
