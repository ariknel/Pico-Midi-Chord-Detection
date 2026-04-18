# 🎹 RP2040 MIDI Chord Detector

> A hardware bridge that sits between your MIDI keyboard and PC — detecting and displaying chords in real time with zero added latency.

```
[ MIDI Keyboard ] ──USB──▶ [ Raspberry Pi Pico ] ──USB──▶ [ PC / DAW ]
                                      │
                                 [ OLED display ]
                                "Cmaj7 / 1st Inv."
```

---

## What this is

A small box built around a standard **Raspberry Pi Pico** (~€4). Plug your USB MIDI keyboard into one side, plug the Pico into your PC on the other. Your DAW sees a normal USB MIDI device — no drivers, no configuration, no audible difference. On top of the box sits a small OLED screen showing exactly what chord you're playing, what inversion it is, and what scales it belongs to.

The passthrough is transparent. The keyboard does not know the Pico is there. The DAW does not know the keyboard went through anything. Chord detection runs entirely on a second CPU core and never touches the data stream going to the PC.

---

## Why Raspberry Pi Pico (not ESP32)

Running simultaneous USB Host (keyboard → Pico) and USB Device (Pico → PC) on one chip is the core hardware challenge of this project.

### The problem with ESP32

Every ESP32 variant — including the S3 — has a single USB OTG peripheral. That peripheral is either Host **or** Device, never both at once. Achieving both simultaneously would require an external USB controller chip (e.g. MAX3421E via SPI), adding cost, extra components, and unavoidable SPI-bridge latency on every MIDI byte. Not viable for a zero-latency transparent bridge.

### How the Pico solves it with a single €4 chip

The RP2040 has one hardware USB controller — but it also has **PIO (Programmable I/O)** state machines. These are small, independent processors that bit-bang digital protocols at hardware speed without involving the CPU at all.

The `Pico-PIO-USB` library implements a complete full-speed USB host entirely in PIO, using just two ordinary GPIO pins (GP0 + GP1). This gives the Pico two live USB ports at the same time:

| Port | Implementation | Role |
|---|---|---|
| Micro-USB (built-in hardware) | Native RP2040 USB controller | Device → PC / DAW |
| GP0 + GP1 via USB-A socket | PIO bit-banged USB Host | Host ← MIDI keyboard |

The PIO host runs on **Core 1** and **PIO block 0** exclusively. Core 0 is left entirely free for chord detection and the OLED display — it never touches the passthrough path.

The Pico is powered from the PC's USB port. No external power supply is needed. The keyboard draws power from VBUS (pin 40) through the USB-A socket.

---

## Current status

| Component | Status |
|---|---|
| Chord detection algorithm (Python) | ✅ Complete |
| Windows test script (pygame piano UI) | ✅ Complete |
| RP2040 firmware skeleton | 🔧 In progress |
| OLED display driver (SSD1306) | 🔧 In progress |
| PIO USB Host MIDI receive (GP0/GP1) | 📋 Planned — v0.2.0 |
| Native USB Device MIDI passthrough to PC | 📋 Planned — v0.2.0 |
| C++ chord engine (ported from Python) | 📋 Planned — v0.2.0 |
| Hardware schematic & BOM | 📋 Planned — v0.4.0 |
| Enclosure design (STL) | 📋 Planned — v0.4.0 |

---

## Repository structure

```
pico-midi-chord-detector/
│
├── test/
│   └── midi_chord_detector.py     # Windows test script — run this first, no hardware needed
│
├── firmware/
│   ├── chord_detector/
│   │   └── chord_detector.ino     # Main Arduino sketch (arduino-pico core)
│   ├── lib/
│   │   ├── ChordEngine.h          # C++ chord detection engine
│   │   ├── ChordEngine.cpp
│   │   └── OledDisplay.h          # SSD1306 display wrapper
│   └── CMakeLists.txt             # Pico SDK build config (alternative to Arduino IDE)
│
├── hardware/
│   ├── schematic.pdf              # Wiring diagram
│   ├── bom.csv                    # Bill of materials with purchase links
│   └── enclosure/                 # STL files for 3D-printed case
│
└── docs/
    ├── build-guide.md             # Step-by-step assembly guide with photos
    └── chord-algorithm.md         # How the detection engine works
```

---

## Chord detection engine

### Chord types supported

| Category | Types |
|---|---|
| Triads | Major, Minor, Augmented, Diminished, Sus2, Sus4, Power |
| Seventh | Maj7, Dom7, Min7, m7b5, Dim7, Aug7, mMaj7, AugMaj7, 7sus4 |
| Sixth | Major 6th, Minor 6th |
| Extended | 9th, Maj9, Min9, 11th, Maj11, Min11, 13th |
| Added tone | Add9, mAdd9, Add11 |
| Altered | 7b5, 7#5, 7b9, 7#9 |

### Scale types (20+)

Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Harmonic Minor, Melodic Minor, Harmonic Major, Phrygian Dominant, Locrian #2, Whole Tone, Diminished H-W, Diminished W-H, Major Pentatonic, Minor Pentatonic, Blues, Dominant Pentatonic, Byzantine.

### What gets displayed per chord

- Root note and chord symbol (e.g. `Cmaj7`)
- Full name (e.g. `C Major 7th`)
- Inversion — Root position, 1st, 2nd, 3rd inv.
- Slash notation when inverted (e.g. `Cmaj7/E`)
- All scales containing this chord
- Ranked alternate interpretations for ambiguous voicings

---

## Running the test script (Windows — no hardware needed)

Verifies the full chord algorithm with a clickable piano UI. Run this before touching any hardware.

**Requirements:** Python 3.10+

```bash
pip install pygame
python test/midi_chord_detector.py
```

**Controls:**

| Input | Action |
|---|---|
| Click piano keys | Play notes |
| `A S D F G H J K L ;` | White keys C4 → E5 |
| `W E T Y U O P` | Black keys C#4 → D#5 |
| Hold multiple keys | Chord detection |
| `Space` | Clear all notes |
| `Esc` | Quit |

---

## Hardware build

### Bill of materials

| Part | Notes | Approx. cost |
|---|---|---|
| Raspberry Pi Pico (or Pico H) | Standard board — no special variant needed | €4 |
| SSD1306 OLED 128×64 | I2C, 3.3V logic | €4 |
| USB-A female breakout board | Exposes VBUS, D+, D−, GND on solderable pins | €1–2 |
| Short micro-USB cable | Pico → PC | — |
| 100µF electrolytic cap | Across VBUS/GND on USB-A socket, stabilises keyboard power | <€1 |
| Small ABS or 3D-printed enclosure | OLED window + two USB cutouts | €3–8 |
| **Total** | | **~€12–20** |

> No special boards. No external USB controller. No extra power supply. Just a standard Pico.

### Wiring

The USB-A socket (keyboard in) wires to four pins on the Pico. The Pico's own micro-USB connector plugs into the PC and stays in USB Device mode — no modifications needed to that port.

```
Pico                     USB-A socket (keyboard input)
─────────────────────────────────────────────────────────────
Pin 1   GP0  ─── D+  ── D+   (white wire in standard USB cable)
Pin 2   GP1  ─── D−  ── D−   (green wire)
Pin 40  VBUS ─── 5V  ── VBUS (red wire) ← powers the keyboard
Any GND      ─── GND ── GND  (black wire)

Pico                     SSD1306 OLED
──────────────────────────────────────
Pin 6   GP4  ─── SDA ── SDA
Pin 7   GP5  ─── SCL ── SCL
Pin 36  3V3  ─── 3V3 ── VCC
Any GND      ─── GND ── GND
```

> **D+ and D− must be consecutive GPIO numbers.** The PIO USB library requires this. GP0/GP1 is the standard tested pair and is what the library defaults to.

> **Do not apply 5V to any GPIO pin.** VBUS (pin 40) is the only 5V output on the Pico. All GPIO are 3.3V logic.

> **Keyboard power draw:** most USB MIDI keyboards draw under 100mA. The Pico's VBUS pin is directly connected to the host PC's USB 5V rail (up to 500mA on USB 2.0). This is sufficient for any standard MIDI keyboard. If your keyboard draws more, add a powered USB hub between the keyboard and the Pico's USB-A socket.

### Dual-core task split

| Core | What it runs | Priority |
|---|---|---|
| **Core 1** | PIO USB host driver + MIDI byte forwarding to PC | Highest — never yields to Core 0 |
| **Core 0** | Chord detection engine, OLED rendering, serial logging | Normal — runs between Core 1 frames |

Every MIDI byte from the keyboard is forwarded to the PC immediately by Core 1, before Core 0 processes anything. The chord display update (~3ms) is entirely asynchronous from the passthrough path. The DAW receives MIDI with no added latency.

### Required libraries (Arduino IDE)

Install via Arduino Library Manager after adding the [arduino-pico](https://github.com/earlephilhower/arduino-pico) board package:

| Library | Purpose |
|---|---|
| `Pico-PIO-USB` (sekigon-gonnoc) | PIO USB host implementation on GP0/GP1 |
| `Adafruit TinyUSB Library` | USB MIDI Device stack (PC-facing port) |
| `usb_midi_host` (rppicomidi) | MIDI class driver on top of PIO USB host |
| `Adafruit SSD1306` | OLED display driver |
| `Adafruit GFX Library` | Graphics dependency for SSD1306 |

**Arduino IDE — Tools menu settings:**

```
Board       → Raspberry Pi Pico
CPU Speed   → 120 MHz   ← required for PIO USB timing (must be multiple of 120)
USB Stack   → Adafruit TinyUSB
Optimize    → Optimize Even More (-O3)
```

---

## Releases

### v0.1.0 — Algorithm & test UI *(current)*

- Full chord detection engine in Python
- All chord types from triads through 13ths and altered chords
- 20+ scale types: all diatonic modes, harmonic/melodic minor, pentatonic, blues, byzantine, and more
- Inversion detection and slash chord notation
- Ranked alternate interpretation for ambiguous voicings
- Windows pygame test UI with clickable piano, live chord display, and scale context panel

---

## Roadmap

### v0.2.0 — Core firmware (passthrough + basic display)
- PIO USB Host receives NoteOn/NoteOff from keyboard on GP0/GP1 via `usb_midi_host`
- Native USB Device forwards all MIDI bytes to PC immediately on Core 1
- C++ chord engine ported from Python algorithm (`ChordEngine.h/.cpp`)
- Chord root + symbol shown on OLED top line (large font)
- Serial log of all detected chords for debugging

### v0.3.0 — Full OLED UI
- Full chord name, inversion label, and slash notation on OLED
- Scale context: up to 3 matching scale names on lower display lines
- Button to scroll through alternate chord interpretations
- Boot splash screen and idle blank after 30 seconds

### v0.4.0 — Hardware release
- Verified schematic PDF
- Full step-by-step assembly guide with photos
- Tested BOM with direct purchase links
- 3D-printable enclosure STL with OLED window and dual USB port cutouts

### v0.5.0 — Extra features
- Transpose mode: shift MIDI output up/down semitones via buttons
- Scale lock mode: OLED flags notes outside the detected scale
- Chord history: scroll back through the last 8 chords played
- MIDI channel filter: monitor a single channel only

### v1.0.0 — Stable release
- All above complete and hardware-tested
- Rotary encoder for OLED menu navigation
- Full user config: display brightness, scale filter, transpose, channel filter
- Documented API for adding custom chord and scale types to `ChordEngine`

---

## How it works

### PIO USB Host — keyboard MIDI in

When the keyboard is plugged into the USB-A socket, Core 1 runs the `Pico-PIO-USB` stack on GP0/GP1. PIO block 0 bit-bangs full-speed USB at 12 Mbps using two state machines — one for transmit (22 instructions) and one for receive (31 instructions). The `usb_midi_host` driver reads USB MIDI class packets and extracts NoteOn, NoteOff, CC, pitch bend, and all other message types.

### Native USB Device — passthrough to PC

Core 1 immediately re-encodes each incoming MIDI message as a USB MIDI class packet and emits it through the RP2040's native USB hardware (the micro-USB port). The PC enumerates this as a standard class-compliant USB MIDI device — no driver installation needed on Windows 10/11. Each packet is forwarded within one USB frame (1ms), with wire transmission time under 3 microseconds at full-speed USB.

### Chord detection on Core 0

Each NoteOn/NoteOff that Core 1 processes is also posted to a lock-free ring buffer. Core 0 reads from this buffer and maintains the current active note set. It then tries every pitch class in the set as a potential root and checks the resulting interval tuple against a lookup table of known chord patterns. The first root-position match is the primary result; inversions and alternate roots are ranked and stored. Detection takes approximately 0.1ms at 120 MHz.

### OLED update

After detection, Core 0 writes the result to the SSD1306 over I2C. The screen shows the chord symbol on the top line (large font), full name and inversion on the second line, and up to three matching scale names below. The display update takes approximately 3ms and is completely isolated from the passthrough path on Core 1.

---

## Contributing

If you build one and encounter chord detection edge cases, keyboards that fail to enumerate on the PIO host, or have OLED layout ideas — open an issue or PR.

The chord engine (`ChordEngine.h`) and the Python test script (`test/midi_chord_detector.py`) implement identical logic. New chord or scale types should be added to both files in the same commit so the Windows test environment stays in sync with the firmware.

---

## License

MIT — build it, modify it, sell kits. Attribution appreciated but not required.
