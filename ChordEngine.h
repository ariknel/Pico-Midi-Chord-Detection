#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "OledDisplay.h"   // for ChordDisplayData, ChordQuality

// ─────────────────────────────────────────────────────────────────────────────
// ChordEngine
//
// Maintains the set of currently held MIDI notes and runs detection
// every time a note changes.
//
// Usage:
//   ChordEngine engine;
//   engine.noteOn(60);   // C4
//   engine.noteOn(64);   // E4
//   engine.noteOn(67);   // G4
//   ChordDisplayData data;
//   engine.fillDisplayData(data);   // fills all fields, sets data.dirty = true
// ─────────────────────────────────────────────────────────────────────────────

// Maximum simultaneous notes we track
#define ENGINE_MAX_NOTES  16
// Maximum chord pattern matches returned
#define ENGINE_MAX_RESULTS 8
// Number of scale types in the table
#define ENGINE_NUM_SCALES  20

// ─── Internal chord match result ─────────────────────────────────────────────
struct ChordMatch {
  uint8_t      root_pc;            // root pitch class 0..11
  uint8_t      pattern_idx;        // index into CHORD_PATTERNS[]
  uint8_t      inversion;          // 0=root, 1=1st, 2=2nd, 3=3rd
  uint8_t      bass_pc;            // actual lowest note pitch class
  ChordQuality quality;
};

class ChordEngine {
public:
  ChordEngine();

  // ── Note input ────────────────────────────────────────────────────────────
  void noteOn (uint8_t midi_note);
  void noteOff(uint8_t midi_note);
  void allNotesOff();

  // ── Query ─────────────────────────────────────────────────────────────────
  // Returns number of matches found (0 if nothing held or no match).
  // Best match is results[0].
  uint8_t detect(ChordMatch results[], uint8_t max_results);

  // Convenience: fill a ChordDisplayData struct from the current state.
  // Sets data.dirty = true whenever the chord changed since last call.
  void fillDisplayData(ChordDisplayData& data);

  // Number of currently held notes
  uint8_t noteCount() const;

  // ── Debug ─────────────────────────────────────────────────────────────────
  // Fills buf with a comma-separated list of active note names, e.g. "C4,E4,G4"
  void activeNotesString(char* buf, uint8_t bufLen);

private:
  bool    _held[128];             // which MIDI notes are currently on
  uint8_t _noteCount;
  bool    _changed;               // true since last fillDisplayData call

  // Cached last result to detect changes
  char    _lastSymbol[16];

  // ── Internal helpers ──────────────────────────────────────────────────────
  void   _buildPitchClasses(uint8_t pcs[], uint8_t& count) const;
  bool   _matchPattern(uint8_t root_pc, uint8_t bass_pc,
                       const uint8_t* pattern, uint8_t plen,
                       uint8_t patternIdx,
                       ChordMatch& out) const;
  void   _buildSymbol (const ChordMatch& m, char* buf, uint8_t bufLen) const;
  void   _buildFullName(uint8_t patternIdx, uint8_t root_pc,
                        char* buf, uint8_t bufLen) const;
  void   _buildInversionLabel(uint8_t inv, char* buf, uint8_t bufLen) const;
  void   _buildNotesString(char* buf, uint8_t bufLen) const;
  uint8_t _findScales(uint8_t root_pc, const uint8_t* pattern, uint8_t plen,
                      char scales[][16], uint8_t maxScales) const;

  static const char* _noteNames[12];
};
