## 🎹 MIDI Chord Display (RP2040)
This project transforms a standard MIDI keyboard (like the SE61) into an live view of the chord we are currently pressing and can select a scale and see whether we are in or out of scale, these states are all saved as functions you can call or return.
Using the RP2040's PIO state machines, we host a USB keyboard and bridge the data to a DAW while calculating chords in real-time.
I will further keep testing until i can get it to function as good as possible.

### 🛠 Hardware Connections
Since the RP2040 native USB port is used for the PC connection, we "bit-bang" a second USB port for the Keyboard.
---

### 🚀 Quick Start Guide

* Add the Pico URL to Boards Manager: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
* Install the **Pico-PIO-USB** library and **Adafruit TinyUSB** library.

#### 2. The Core Code Structure

```cpp
#include <Adafruit_TinyUSB.h>
#include <pio_usb.h>

// USB Host for the Keyboard (PIO)
Adafruit_USBH_MIDI usb_host_midi;
// USB Device for the PC (Native)
Adafruit_USBD_MIDI usb_device_midi;

void setup() {
  // Initialize the PC connection (DAW)
  usb_device_midi.begin();
  
  // Initialize the Keyboard connection (Host)
  // Pin 0 is D+, Pin 1 is D-
  pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
  config.pin_dp = 0; 
  USBHost.begin(0);
  
  Serial.begin(115200);
}

void loop() {
  // Keep the USB Host hardware alive
  USBHost.task();

  // 1. Listen for Note from SE61
  if (usb_host_midi.available()) {
    uint8_t packet[4];
    if (usb_host_midi.read(packet)) {
      
      // 2. RUN CHORD ENGINE
      // Example: Detect if packet[1] (status) is Note On
      process_chord_logic(packet);

      // 3. Forward to DAW
      usb_device_midi.write(packet, 4);
    }
  }
}

void process_chord_logic(uint8_t* midi_data) {
    // Your detection logic goes here!
}
```

---

### 📝 To-Do List for v1.0
* [ ] Implement Note-On/Note-Off tracking array.
* [ ] Map intervals to chord names (e.g., +4, +7 = Major).
* [ ] Add OLED I2C display support for chord names.
* [ ] Optimize PIO timing for high-speed play.

---

### 🤝 Contributing
Feel free to fork this repo and add your own chord voicings or scales!

---
