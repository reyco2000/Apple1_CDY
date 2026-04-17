/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        memory.cpp
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Apple I memory bus implementation. Routes CPU
 *              reads/writes through the address decoder to RAM,
 *              PIA registers, and ROM. Includes optional debug
 *              tracing for unmapped memory accesses.
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

#include "memory.h"
#include "roms.h"

// Wait, I should not `#include <Arduino.h>` directly if I am just doing raw C++, but pgm_read_byte needs Arduino.h.
#include <Arduino.h>

Apple1Memory::Apple1Memory(Apple1PIA* pia_ptr) : pia(pia_ptr) {
    debug = false;
    reset();
}

void Apple1Memory::reset() {
    for (int i = 0; i < RAM_SIZE; i++) {
        ram[i] = 0;
    }
}

uint8_t Apple1Memory::read(uint16_t address) {
    if (address < RAM_SIZE) {
        return ram[address];
    }
    
    // PIA
    if (address >= 0xD010 && address <= 0xD013) {
        return pia->read(address);
    }
    
    // BASIC ROM ($E000-$EFFF)
    if (address >= 0xE000 && address <= 0xEFFF) {
        return pgm_read_byte(&APPLE1_BASIC[address - 0xE000]);
    }
    
    // WOZ Monitor ($FF00-$FFFF)
    if (address >= 0xFF00 && address <= 0xFFFF) {
        return pgm_read_byte(&WOZ_MONITOR[address - 0xFF00]);
    }
    
    // Debug: reading from unmapped memory  
    if (debug && address >= 0x2000 && address < 0xD010) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[MEM] Read unmapped addr=%04X ret=FF", address);
        Serial.println(buf);
    }
    
    return 0xFF; // Open bus
}

void Apple1Memory::write(uint16_t address, uint8_t value) {
    if (address < RAM_SIZE) {
        ram[address] = value;
        return;
    }
    
    // PIA
    if (address >= 0xD010 && address <= 0xD013) {
        pia->write(address, value);
        return;
    }
    
    // Debug: write to unmapped address
    if (debug && address >= RAM_SIZE && !(address >= 0xD010 && address <= 0xD013)) {
        char buf[60];
        snprintf(buf, sizeof(buf), "[MEM] Write LOST! addr=%04X val=%02X", address, value);
        Serial.println(buf);
    }
    
    // ROM is read-only
}

uint16_t Apple1Memory::readWord(uint16_t address) {
    uint8_t lo = read(address);
    uint8_t hi = read(address + 1);
    return (hi << 8) | lo;
}
