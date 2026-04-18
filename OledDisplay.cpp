#include "OledDisplay.h"
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
OledDisplay::OledDisplay()
  : _disp(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET),
    _initialised(false)
{}

// ─────────────────────────────────────────────────────────────────────────────
// begin() — call once in setup()
// Returns false if the display does not respond on I2C
// ─────────────────────────────────────────────────────────────────────────────
bool OledDisplay::begin() {
  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();

  if (!_disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    return false;
  }

  _initialised = true;
  _disp.setTextColor(SSD1306_WHITE);
  _disp.clearDisplay();

  showSplash();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// showSplash() — boot screen
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::showSplash() {
  if (!_initialised) return;

  _disp.clearDisplay();

  // Top border
  _disp.drawFastHLine(0, 0, OLED_WIDTH, SSD1306_WHITE);

  // Title
  _disp.setTextSize(1);
  _disp.setCursor(8, 5);
  _disp.print(F("MIDI Chord Detector"));

  // Sub
  _disp.setCursor(26, 17);
  _disp.print(F("RP2040  v0.1.0"));

  // Middle border
  _disp.drawFastHLine(0, 27, OLED_WIDTH, SSD1306_WHITE);

  // Bottom info
  _disp.setCursor(14, 34);
  _disp.print(F("Wokwi simulation"));
  _disp.setCursor(22, 46);
  _disp.print(F("OledDisplay.h/cpp"));

  // Bottom border
  _disp.drawFastHLine(0, 58, OLED_WIDTH, SSD1306_WHITE);

  _disp.display();
  delay(1800);
}

// ─────────────────────────────────────────────────────────────────────────────
// update() — only redraws when data.dirty == true
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::update(const ChordDisplayData& data) {
  if (!_initialised || !data.dirty) return;
  forceRedraw(data);
}

// ─────────────────────────────────────────────────────────────────────────────
// forceRedraw() — always redraws
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::forceRedraw(const ChordDisplayData& data) {
  if (!_initialised) return;

  _disp.clearDisplay();

  if (!data.has_chord) {
    _drawIdle();
  } else {
    _drawChordSymbol(data);
    _drawDivider(19);
    _drawNameLine(data);
    _drawDivider(30);
    _drawNotesLine(data);
    _drawDivider(41);
    _drawScales(data);
  }

  _disp.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// clear()
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::clear() {
  if (!_initialised) return;
  _disp.clearDisplay();
  _disp.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawChordSymbol  — zone y=0..18
// Uses size-3 font (18px) for short symbols, size-2 for long ones
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawChordSymbol(const ChordDisplayData& data) {
  uint8_t len = strlen(data.symbol);

  if (len == 0) return;

  if (len <= 5) {
    // Size 3: each char is 18px tall, ~18px wide
    _disp.setTextSize(3);
    _disp.setCursor(0, 0);
  } else if (len <= 8) {
    // Size 2: each char is 12px tall, ~12px wide
    _disp.setTextSize(2);
    _disp.setCursor(0, 2);
  } else {
    // Size 1 fallback for very long symbols
    _disp.setTextSize(1);
    _disp.setCursor(0, 5);
  }

  _disp.print(data.symbol);
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawDivider — 1px horizontal rule at y
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawDivider(int y) {
  _disp.drawFastHLine(0, y, OLED_WIDTH, SSD1306_WHITE);
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawNameLine — zone y=20..28
// "C Major 7th     1st inv."
// Full name left-aligned, inversion right-aligned
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawNameLine(const ChordDisplayData& data) {
  _disp.setTextSize(1);

  // Each char at size 1 = 6px wide. 128px / 6 = ~21 chars per line.
  // Strategy: full_name left, inversion pushed right if it fits.
  uint8_t nameLen = strlen(data.full_name);
  uint8_t invLen  = strlen(data.inversion);

  _disp.setCursor(0, 21);
  _disp.print(data.full_name);

  if (invLen > 0) {
    // Right-align the inversion label
    int16_t invX = OLED_WIDTH - (invLen * 6);
    if (invX > nameLen * 6 + 2) {
      // Enough room — draw it
      _disp.setCursor(invX, 21);
      _disp.print(data.inversion);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawNotesLine — zone y=30..38
// "  C4 E4 G4 B4"
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawNotesLine(const ChordDisplayData& data) {
  if (strlen(data.notes_str) == 0) return;

  _disp.setTextSize(1);
  _disp.setCursor(0, 32);

  // Small note indicator glyph (two pixels wide dash)
  _disp.drawPixel(0, 34, SSD1306_WHITE);
  _disp.drawPixel(1, 34, SSD1306_WHITE);
  _disp.drawPixel(0, 35, SSD1306_WHITE);
  _disp.drawPixel(1, 35, SSD1306_WHITE);
  _disp.drawPixel(0, 36, SSD1306_WHITE);
  _disp.drawPixel(1, 36, SSD1306_WHITE);

  _disp.setCursor(4, 32);
  _disp.print(data.notes_str);
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawScales — zone y=41..63
// Up to 4 scales in a 2×2 grid
//
//  Col 0 (x=0)     Col 1 (x=64)
//  row 0 (y=43)    scale[0]    scale[1]
//  row 1 (y=53)    scale[2]    scale[3]
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawScales(const ChordDisplayData& data) {
  if (data.num_scales == 0) {
    _disp.setTextSize(1);
    _disp.setCursor(0, 43);
    _disp.print(F("No scale match"));
    return;
  }

  _disp.setTextSize(1);

  // Layout: two columns, two rows
  // Max chars per cell: 64px / 6px = ~10 chars
  const int colX[2] = { 0, 64 };
  const int rowY[2] = { 43, 53 };

  for (uint8_t i = 0; i < data.num_scales && i < 4; i++) {
    int col = i % 2;
    int row = i / 2;

    char cell[11];
    _truncate(cell, data.scales[i], 10);

    _disp.setCursor(colX[col], rowY[row]);
    _disp.print(cell);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawIdle — shown when no chord is active
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawIdle() {
  _disp.setTextSize(1);
  _disp.setCursor(22, 4);
  _disp.print(F("Play a chord..."));

  // Mini piano keyboard graphic
  _drawSmallKeyboard(16, 18);

  _disp.setCursor(14, 55);
  _disp.print(F("MIDI Chord Detect"));
}

// ─────────────────────────────────────────────────────────────────────────────
// _drawSmallKeyboard — draws 7 white keys + 5 black keys
// Starts at pixel (x, y), each white key is 13×24px
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_drawSmallKeyboard(int x, int y) {
  const int keyW  = 13;
  const int keyH  = 24;
  const int blackW = 8;
  const int blackH = 14;

  // White keys
  for (int k = 0; k < 7; k++) {
    _disp.drawRect(x + k * keyW, y, keyW - 1, keyH, SSD1306_WHITE);
  }

  // Black keys — offsets relative to left edge of white key start
  // Between keys: C#, D#, (gap), F#, G#, A#
  const int blackOffsets[] = { 8, 21, 47, 60, 73 };
  for (int i = 0; i < 5; i++) {
    _disp.fillRect(x + blackOffsets[i], y, blackW, blackH, SSD1306_WHITE);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _truncate — safe string copy with max char count, always null-terminated
// ─────────────────────────────────────────────────────────────────────────────
void OledDisplay::_truncate(char* dest, const char* src, uint8_t maxChars) {
  strncpy(dest, src, maxChars);
  dest[maxChars] = '\0';
}
