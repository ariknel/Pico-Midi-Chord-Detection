#include "ChordEngine.h"
#include <string.h>
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
// Static lookup tables (stored in flash via PROGMEM on AVR; fine on RP2040)
// ─────────────────────────────────────────────────────────────────────────────

// Note names — sharps only (detection handles enharmonics contextually)
const char* ChordEngine::_noteNames[12] = {
  "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

// ── Chord pattern table ───────────────────────────────────────────────────────
// Each entry: { intervals[], length, symbol_suffix, full_name, quality }
// intervals are semitones from root (root = 0 always included implicitly)
struct ChordPattern {
  uint8_t      intervals[6];   // semitones from root, not including root itself
  uint8_t      len;            // number of intervals (excluding root)
  const char*  suffix;         // appended after root name, e.g. "maj7"
  const char*  full_name;      // e.g. "Major 7th"
  ChordQuality quality;
};

static const ChordPattern CHORD_PATTERNS[] = {
  // ── Triads ──────────────────────────────────────────────────────────────
  {{ 4, 7},           2, "",      "Major",          QUALITY_MAJOR      },
  {{ 3, 7},           2, "m",     "Minor",          QUALITY_MINOR      },
  {{ 4, 8},           2, "aug",   "Augmented",      QUALITY_AUGMENTED  },
  {{ 3, 6},           2, "dim",   "Diminished",     QUALITY_DIMINISHED },
  {{ 5, 7},           2, "sus4",  "Suspended 4th",  QUALITY_SUSPENDED  },
  {{ 2, 7},           2, "sus2",  "Suspended 2nd",  QUALITY_SUSPENDED  },
  {{ 7},              1, "5",     "Power chord",    QUALITY_POWER      },

  // ── Seventh chords ────────────────────────────────────────────────────────
  {{ 4, 7,11},        3, "maj7",  "Major 7th",      QUALITY_MAJOR      },
  {{ 4, 7,10},        3, "7",     "Dominant 7th",   QUALITY_DOMINANT   },
  {{ 3, 7,10},        3, "m7",    "Minor 7th",      QUALITY_MINOR      },
  {{ 3, 6,10},        3, "m7b5",  "Half-Dim 7th",   QUALITY_DIMINISHED },
  {{ 3, 6, 9},        3, "dim7",  "Diminished 7th", QUALITY_DIMINISHED },
  {{ 4, 8,10},        3, "aug7",  "Augmented 7th",  QUALITY_AUGMENTED  },
  {{ 3, 7,11},        3, "mMaj7", "Minor Major 7th",QUALITY_MINOR      },
  {{ 5, 7,10},        3, "7sus4", "Dom 7th Sus4",   QUALITY_SUSPENDED  },

  // ── Sixth chords ─────────────────────────────────────────────────────────
  {{ 4, 7, 9},        3, "6",     "Major 6th",      QUALITY_MAJOR      },
  {{ 3, 7, 9},        3, "m6",    "Minor 6th",      QUALITY_MINOR      },

  // ── Extended ─────────────────────────────────────────────────────────────
  {{ 4, 7,10,14},     4, "9",     "Dominant 9th",   QUALITY_DOMINANT   },
  {{ 4, 7,11,14},     4, "maj9",  "Major 9th",      QUALITY_MAJOR      },
  {{ 3, 7,10,14},     4, "m9",    "Minor 9th",      QUALITY_MINOR      },
  {{ 4, 7,10,14,17},  5, "11",    "Dominant 11th",  QUALITY_DOMINANT   },
  {{ 3, 7,10,14,17},  5, "m11",   "Minor 11th",     QUALITY_MINOR      },

  // ── Added tone ────────────────────────────────────────────────────────────
  {{ 4, 7,14},        3, "add9",  "Add 9",          QUALITY_MAJOR      },
  {{ 3, 7,14},        3, "madd9", "Minor Add 9",    QUALITY_MINOR      },

  // ── Altered ──────────────────────────────────────────────────────────────
  {{ 4, 6,10},        3, "7b5",   "Dom 7th b5",     QUALITY_DOMINANT   },
  {{ 4, 8,10},        3, "7#5",   "Dom 7th #5",     QUALITY_DOMINANT   },
};

static const uint8_t NUM_CHORD_PATTERNS =
  sizeof(CHORD_PATTERNS) / sizeof(CHORD_PATTERNS[0]);

// ── Scale table ───────────────────────────────────────────────────────────────
// intervals from root (not including root itself)
struct ScalePattern {
  uint8_t      intervals[7];
  uint8_t      len;
  const char*  name;           // short name for display (max 15 chars)
};

static const ScalePattern SCALE_PATTERNS[ENGINE_NUM_SCALES] = {
  {{ 2, 4, 5, 7, 9,11},   6, "Ionian"    },
  {{ 2, 3, 5, 7, 9,10},   6, "Dorian"    },
  {{ 1, 3, 5, 7, 8,10},   6, "Phrygian"  },
  {{ 2, 4, 6, 7, 9,11},   6, "Lydian"    },
  {{ 2, 4, 5, 7, 9,10},   6, "Mixolydian"},
  {{ 2, 3, 5, 7, 8,10},   6, "Aeolian"   },
  {{ 1, 3, 5, 6, 8,10},   6, "Locrian"   },
  {{ 2, 3, 5, 7, 8,11},   6, "HarmMinor" },
  {{ 2, 3, 5, 7, 9,11},   6, "MelMinor"  },
  {{ 2, 4, 6, 8,10},      5, "WholeTone" },
  {{ 1, 3, 4, 6, 7, 9,10},7, "Dim(HW)"   },
  {{ 2, 3, 5, 6, 8, 9,11},7, "Dim(WH)"   },
  {{ 2, 4, 7, 9},         4, "MajPent"   },
  {{ 3, 5, 7,10},         4, "MinPent"   },
  {{ 3, 5, 6, 7,10},      5, "Blues"     },
  {{ 2, 4, 5, 7, 8,11},   6, "HarmMajor" },
  {{ 1, 4, 5, 7, 8,11},   6, "Byzantine" },
  {{ 2, 3, 5, 7, 8,10},   6, "PhrygDom"  },
  {{ 2, 4, 7, 9,10},      5, "DomPent"   },
  {{ 2, 3, 5, 7, 9,11},   6, "MelMajor"  },
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
ChordEngine::ChordEngine() : _noteCount(0), _changed(false) {
  memset(_held, 0, sizeof(_held));
  memset(_lastSymbol, 0, sizeof(_lastSymbol));
}

// ─────────────────────────────────────────────────────────────────────────────
// noteOn / noteOff / allNotesOff
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::noteOn(uint8_t midi_note) {
  if (midi_note > 127) return;
  if (!_held[midi_note]) {
    _held[midi_note] = true;
    _noteCount++;
    _changed = true;
  }
}

void ChordEngine::noteOff(uint8_t midi_note) {
  if (midi_note > 127) return;
  if (_held[midi_note]) {
    _held[midi_note] = false;
    _noteCount--;
    _changed = true;
  }
}

void ChordEngine::allNotesOff() {
  memset(_held, 0, sizeof(_held));
  _noteCount = 0;
  _changed = true;
}

uint8_t ChordEngine::noteCount() const {
  return _noteCount;
}

// ─────────────────────────────────────────────────────────────────────────────
// detect()
// Tries every pitch class as a potential root, checks against every
// chord pattern. Returns sorted results (root position first, then
// inversions; longer patterns ranked higher than shorter ones).
// ─────────────────────────────────────────────────────────────────────────────
uint8_t ChordEngine::detect(ChordMatch results[], uint8_t max_results) {
  if (_noteCount < 1) return 0;

  uint8_t pcs[12];
  uint8_t pcCount = 0;
  _buildPitchClasses(pcs, pcCount);

  if (pcCount == 0) return 0;

  // Find bass note (lowest held MIDI note)
  uint8_t bass_pc = 255;
  for (int n = 0; n < 128; n++) {
    if (_held[n]) { bass_pc = n % 12; break; }
  }

  uint8_t found = 0;

  // Try every pitch class as root
  for (uint8_t r = 0; r < pcCount && found < max_results; r++) {
    uint8_t root = pcs[r];

    // Build intervals from this root
    uint8_t ivs[12];
    uint8_t ivcnt = 0;
    for (uint8_t i = 0; i < pcCount; i++) {
      uint8_t iv = (pcs[i] - root + 12) % 12;
      if (iv != 0) ivs[ivcnt++] = iv;
    }

    // Sort intervals
    for (uint8_t a = 0; a < ivcnt - 1; a++) {
      for (uint8_t b = a + 1; b < ivcnt; b++) {
        if (ivs[b] < ivs[a]) { uint8_t t = ivs[a]; ivs[a] = ivs[b]; ivs[b] = t; }
      }
    }

    // Check every chord pattern
    for (uint8_t p = 0; p < NUM_CHORD_PATTERNS && found < max_results; p++) {
      if (CHORD_PATTERNS[p].len != ivcnt) continue;

      bool match = true;
      for (uint8_t k = 0; k < ivcnt; k++) {
        if (ivs[k] != CHORD_PATTERNS[p].intervals[k]) { match = false; break; }
      }

      if (match) {
        ChordMatch m;
        m.root_pc    = root;
        m.pattern_idx = p;
        m.bass_pc    = bass_pc;
        m.quality    = CHORD_PATTERNS[p].quality;

        // Determine inversion
        m.inversion = 0;
        if (bass_pc != root) {
          for (uint8_t k = 0; k < CHORD_PATTERNS[p].len; k++) {
            if ((root + CHORD_PATTERNS[p].intervals[k]) % 12 == bass_pc) {
              m.inversion = k + 1;
              break;
            }
          }
        }

        results[found++] = m;
      }
    }
  }

  // Sort: root position before inversions; then by pattern length (longer = richer)
  for (uint8_t a = 0; a < found - 1; a++) {
    for (uint8_t b = a + 1; b < found; b++) {
      bool swapIt = false;
      if (results[b].inversion < results[a].inversion) {
        swapIt = true;
      } else if (results[b].inversion == results[a].inversion) {
        if (CHORD_PATTERNS[results[b].pattern_idx].len >
            CHORD_PATTERNS[results[a].pattern_idx].len) {
          swapIt = true;
        }
      }
      if (swapIt) { ChordMatch t = results[a]; results[a] = results[b]; results[b] = t; }
    }
  }

  return found;
}

// ─────────────────────────────────────────────────────────────────────────────
// fillDisplayData()
// Runs detect(), populates ChordDisplayData, sets dirty only on change
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::fillDisplayData(ChordDisplayData& data) {
  ChordMatch results[ENGINE_MAX_RESULTS];
  uint8_t n = detect(results, ENGINE_MAX_RESULTS);

  if (_noteCount == 0 || n == 0) {
    data.has_chord  = false;
    data.dirty      = (_changed || strcmp(_lastSymbol, "") != 0);
    _changed        = false;
    memset(_lastSymbol, 0, sizeof(_lastSymbol));
    data.symbol[0]  = '\0';
    data.full_name[0] = '\0';
    data.inversion[0] = '\0';
    data.notes_str[0] = '\0';
    data.num_scales   = 0;
    data.quality      = QUALITY_UNKNOWN;
    return;
  }

  const ChordMatch& best = results[0];
  const ChordPattern& pat = CHORD_PATTERNS[best.pattern_idx];

  // Build symbol (e.g. "Cmaj7/E")
  _buildSymbol(best, data.symbol, sizeof(data.symbol));

  // Has it changed?
  bool changed = (strcmp(data.symbol, _lastSymbol) != 0) || _changed;
  data.dirty   = changed;
  _changed     = false;

  if (!changed) return;   // same chord — no need to rebuild everything

  strncpy(_lastSymbol, data.symbol, sizeof(_lastSymbol) - 1);

  // Full name (e.g. "C Major 7th")
  _buildFullName(best.pattern_idx, best.root_pc, data.full_name, sizeof(data.full_name));

  // Inversion label
  _buildInversionLabel(best.inversion, data.inversion, sizeof(data.inversion));

  // Active notes string
  _buildNotesString(data.notes_str, sizeof(data.notes_str));

  // Scales
  data.num_scales = _findScales(best.root_pc,
                                pat.intervals, pat.len,
                                data.scales, 4);

  data.quality   = best.quality;
  data.has_chord = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// activeNotesString() — debug helper
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::activeNotesString(char* buf, uint8_t bufLen) {
  _buildNotesString(buf, bufLen);
}

// ─────────────────────────────────────────────────────────────────────────────
// _buildPitchClasses — collect unique pitch classes from held notes
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::_buildPitchClasses(uint8_t pcs[], uint8_t& count) const {
  bool seen[12] = {};
  count = 0;
  for (int n = 0; n < 128; n++) {
    if (_held[n]) {
      uint8_t pc = n % 12;
      if (!seen[pc]) {
        seen[pc] = true;
        pcs[count++] = pc;
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _matchPattern — unused in current flow but kept for future direct use
// ─────────────────────────────────────────────────────────────────────────────
bool ChordEngine::_matchPattern(uint8_t root_pc, uint8_t bass_pc,
                                 const uint8_t* pattern, uint8_t plen,
                                 uint8_t patternIdx, ChordMatch& out) const {
  (void)root_pc; (void)bass_pc; (void)pattern; (void)plen;
  (void)patternIdx; (void)out;
  return false; // placeholder
}

// ─────────────────────────────────────────────────────────────────────────────
// _buildSymbol — e.g. "Cmaj7", or "Cmaj7/E" if inverted
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::_buildSymbol(const ChordMatch& m, char* buf, uint8_t bufLen) const {
  const ChordPattern& pat = CHORD_PATTERNS[m.pattern_idx];
  const char* root = _noteNames[m.root_pc];

  if (m.inversion == 0) {
    snprintf(buf, bufLen, "%s%s", root, pat.suffix);
  } else {
    const char* bass = _noteNames[m.bass_pc];
    snprintf(buf, bufLen, "%s%s/%s", root, pat.suffix, bass);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _buildFullName — e.g. "C Major 7th"
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::_buildFullName(uint8_t patternIdx, uint8_t root_pc,
                                  char* buf, uint8_t bufLen) const {
  snprintf(buf, bufLen, "%s %s",
           _noteNames[root_pc],
           CHORD_PATTERNS[patternIdx].full_name);
}

// ─────────────────────────────────────────────────────────────────────────────
// _buildInversionLabel
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::_buildInversionLabel(uint8_t inv, char* buf, uint8_t bufLen) const {
  const char* labels[] = { "Root pos.", "1st inv.", "2nd inv.", "3rd inv.", "4th inv." };
  uint8_t idx = (inv < 5) ? inv : 4;
  strncpy(buf, labels[idx], bufLen - 1);
  buf[bufLen - 1] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// _buildNotesString — e.g. "C4 E4 G4 B4"
// ─────────────────────────────────────────────────────────────────────────────
void ChordEngine::_buildNotesString(char* buf, uint8_t bufLen) const {
  buf[0] = '\0';
  uint8_t pos = 0;

  for (int n = 0; n < 128 && pos < bufLen - 5; n++) {
    if (_held[n]) {
      int octave = (n / 12) - 1;
      const char* name = _noteNames[n % 12];

      char tmp[6];
      snprintf(tmp, sizeof(tmp), "%s%d ", name, octave);

      uint8_t tlen = strlen(tmp);
      if (pos + tlen < bufLen) {
        strcat(buf, tmp);
        pos += tlen;
      }
    }
  }

  // Trim trailing space
  uint8_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == ' ') buf[len - 1] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// _findScales — find scales containing all notes of this chord
// ─────────────────────────────────────────────────────────────────────────────
uint8_t ChordEngine::_findScales(uint8_t root_pc, const uint8_t* pattern,
                                  uint8_t plen, char scales[][16],
                                  uint8_t maxScales) const {
  uint8_t found = 0;

  // Build the set of chord pitch classes
  bool chordPCs[12] = {};
  chordPCs[root_pc] = true;
  for (uint8_t i = 0; i < plen; i++) {
    chordPCs[(root_pc + pattern[i]) % 12] = true;
  }

  // For every scale root and every scale type, check if chord PCs ⊆ scale PCs
  for (uint8_t sr = 0; sr < 12 && found < maxScales; sr++) {
    for (uint8_t sp = 0; sp < ENGINE_NUM_SCALES && found < maxScales; sp++) {
      bool scalePCs[12] = {};
      scalePCs[sr] = true;
      for (uint8_t i = 0; i < SCALE_PATTERNS[sp].len; i++) {
        scalePCs[(sr + SCALE_PATTERNS[sp].intervals[i]) % 12] = true;
      }

      // Check all chord PCs are in scale PCs
      bool fits = true;
      for (uint8_t pc = 0; pc < 12; pc++) {
        if (chordPCs[pc] && !scalePCs[pc]) { fits = false; break; }
      }

      if (fits) {
        // Format: "C Ionian" — max 15 chars
        snprintf(scales[found], 16, "%s %s",
                 _noteNames[sr], SCALE_PATTERNS[sp].name);
        found++;
      }
    }
  }

  return found;
}
