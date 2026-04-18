#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// ─── Pin config ───────────────────────────────────────────────────────────────
#define OLED_SDA      4
#define OLED_SCL      5
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_ADDR     0x3C
#define OLED_RESET    -1

// ─── Quality enum — maps to display style / label ────────────────────────────
enum ChordQuality : uint8_t {
  QUALITY_MAJOR      = 0,
  QUALITY_MINOR      = 1,
  QUALITY_DOMINANT   = 2,
  QUALITY_DIMINISHED = 3,
  QUALITY_AUGMENTED  = 4,
  QUALITY_SUSPENDED  = 5,
  QUALITY_POWER      = 6,
  QUALITY_UNKNOWN    = 7
};

// ─── The display data contract ───────────────────────────────────────────────
// OledDisplay reads ONLY from this struct.
// ChordEngine writes ONLY to this struct.
// They never call each other directly.
struct ChordDisplayData {
  char          symbol[16];       // "Cmaj7/E"        — chord name + slash bass
  char          full_name[32];    // "C Major 7th"    — human readable
  char          inversion[14];    // "1st inv."       — inversion label
  char          notes_str[24];    // "C4 E4 G4 B4"   — active notes, space sep
  char          scales[4][16];    // up to 4 matching scale names
  uint8_t       num_scales;       // how many scales[] entries are valid
  ChordQuality  quality;          // drives visual style
  bool          has_chord;        // false = idle (no notes held)
  bool          dirty;            // true = display needs redraw
};

// ─── OledDisplay class ───────────────────────────────────────────────────────
class OledDisplay {
public:
  OledDisplay();

  // Call once in setup()
  bool begin();

  // Call in loop() — redraws only when data.dirty is true
  void update(const ChordDisplayData& data);

  // Force a full redraw regardless of dirty flag
  void forceRedraw(const ChordDisplayData& data);

  // Show the boot splash (called automatically by begin())
  void showSplash();

  // Clear to black
  void clear();

private:
  Adafruit_SSD1306 _disp;
  bool             _initialised;

  // Zone renderers — each owns a pixel region
  void _drawChordSymbol  (const ChordDisplayData& data);  // y 0..18
  void _drawDivider      (int y);
  void _drawNameLine     (const ChordDisplayData& data);  // y 20..28
  void _drawNotesLine    (const ChordDisplayData& data);  // y 30..38
  void _drawScales       (const ChordDisplayData& data);  // y 41..63
  void _drawIdle         ();

  // Helpers
  void _truncate         (char* dest, const char* src, uint8_t maxChars);
  void _drawSmallKeyboard(int x, int y);
};
