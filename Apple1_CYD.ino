/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        Apple1_CYD.ino
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Main sketch — initializes the ESP32 CYD hardware,
 *              wires up the emulated CPU, memory, PIA, and display,
 *              then runs the emulation loop at 60 FPS. Handles
 *              serial terminal I/O with CR+LF filtering and
 *              peek-based input buffering for reliable paste support.
 *
 * Connect with the Author:
 * YouTube:     ChipShift (https://www.youtube.com/@ChipShift)
 * -------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * -------------------------------------------------------------------
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "cpu6502.h"
#include "display.h"
#include "memory.h"
#include "pia.h"
#include "roms.h"

// TFT setup using standard TFT_eSPI instance
TFT_eSPI tft = TFT_eSPI();

Apple1Display *display;
Apple1PIA *pia;
Apple1Memory *memory;
CPU6502 *cpu;

unsigned long last_frame_time = 0;
const int FPS = 60;
const int CYCLES_PER_FRAME = 1000000 / FPS;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("Initializing Apple I Emulator for ESP32 CYD...");

  // Initialize TFT
  tft.init();
  tft.setRotation(1); // Inverted Portrait natively Maps to upright view for
                      // this physical CYD

  // Instantiate hardware components
  display = new Apple1Display(&tft);
  pia = new Apple1PIA();
  memory = new Apple1Memory(pia);
  cpu = new CPU6502(memory);

  // Wire up dependencies
  pia->setDisplay(display);

  // Initial state
  display->init();
  pia->reset();
  memory->reset();

  // Boot CPU (Memory automatically reads RESET_VECTOR from WOZ_MONITOR)
  cpu->reset();

  Serial.println("System booted. Send characters via Serial monitor.");
  Serial.println("*** Send ~ (tilde) to toggle CPU debug tracing ***");
  last_frame_time = millis();
}

void loop() {
  // 1. Process Serial Input (Keyboard)
  //    Use peek() to avoid consuming characters the PIA can't accept yet.
  //    This prevents data loss when pasting large programs.
  while (Serial.available()) {
    char c = Serial.peek();
    
    // Always consume and discard LF — Apple 1 uses CR only
    if (c == 0x0A) {
      Serial.read();
      continue;  // Check for more buffered LFs
    }
    
    // Always consume debug toggle
    if (c == '~') {
      Serial.read();
      cpu->debug = !cpu->debug;
      memory->debug = cpu->debug;
      Serial.print("*** DEBUG ");
      Serial.println(cpu->debug ? "ON ***" : "OFF ***");
      cpu->dumpState();
      break;
    }
    
    // Real character — only consume if PIA can accept it
    if (!pia->isKeyReady()) {
      Serial.read();
      pia->keyPress((uint8_t)c);
    }
    break;  // Process at most one real character per loop iteration
  }

  // 2. Emulate 6502 for 1 frame's worth of cycles (~16.6 ms)
  int cycles_this_frame = 0;
  while (cycles_this_frame < CYCLES_PER_FRAME) {
    cycles_this_frame += cpu->step();
  }

  // 3. Render screen updates
  display->refresh();

  // 4. Regulate framerate/clock speed
  unsigned long now = millis();
  unsigned long frame_duration = now - last_frame_time;
  if (frame_duration < (1000 / FPS)) {
    delay((1000 / FPS) - frame_duration);
  }
  last_frame_time = millis();
}
