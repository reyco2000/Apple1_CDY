/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        pia.cpp
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: MC6821 PIA emulation. Implements register read/write
 *              for keyboard data (Port A) and display output
 *              (Port B), including IRQ flag management, data
 *              direction registers, and uppercase conversion for
 *              Apple I compatibility.
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

#include "pia.h"
#include "display.h"

Apple1PIA::Apple1PIA() {
    display = nullptr;
    reset();
}

void Apple1PIA::setDisplay(Apple1Display* disp) {
    display = disp;
}

void Apple1PIA::reset() {
    DDRA = 0; CRA = 0; ORA = 0;
    DDRB = 0; CRB = 0; ORB = 0;
    last_key = 0;
    key_ready = false;
    display_ready = true;
}

void Apple1PIA::keyPress(uint8_t key) {
    if (key >= 0x61 && key <= 0x7A) {
        key -= 0x20; // Convert to uppercase for Apple I
    }
    if (key == 0x0A) key = 0x0D; // LF -> CR (safety fallback)
    
    last_key = key & 0x7F;
    key_ready = true;
    
    // In full emulation we trigger CA1 interrupt here. 
    // Apple 1 monitor doesn't really use IRQ for keyboard (it polls D011), but we set the bit.
    CRA |= 0x80; // IRQ1 flag
}

uint8_t Apple1PIA::read(uint16_t address) {
    uint8_t reg = address & 0x03;
    
    if (reg == 0) { // Keyboard data (Port A)
        if (CRA & 0x04) { // Data Register
            uint8_t val = last_key;
            if (key_ready) {
                val |= 0x80; // Bit 7 set when key pushed
                key_ready = false;
                CRA &= ~0x80; // Clear IRQ1
            }
            return val;
        } else {
            return DDRA;
        }
    } 
    else if (reg == 1) { // Keyboard control (CRA)
        uint8_t val = CRA & 0x7F;
        if (key_ready) {
            val |= 0x80;
        }
        return val;
    }
    else if (reg == 2) { // Display data (Port B)
        if (CRB & 0x04) {
            uint8_t val = ORB & 0x7F;
            if (!display_ready) {
                val |= 0x80; // Busy
            }
            CRB &= ~0x80; // Clear IRQ1 on read
            return val;
        } else {
            return DDRB;
        }
    }
    else if (reg == 3) { // Display control (CRB)
        uint8_t val = CRB;
        if (display_ready) {
            val |= 0x80; // Display ready flag for polling loop
        }
        return val;
    }
    return 0;
}

void Apple1PIA::write(uint16_t address, uint8_t value) {
    uint8_t reg = address & 0x03;
    
    if (reg == 0) {
        if (CRA & 0x04) {
            ORA = value;
        } else {
            DDRA = value;
        }
    }
    else if (reg == 1) {
        CRA = (CRA & 0xC0) | (value & 0x3F);
    }
    else if (reg == 2) {
        if (CRB & 0x04) {
            ORB = value;
            if (display) {
                display->writeChar(value & 0x7F);
            }
            display_ready = true;
        } else {
            DDRB = value;
        }
    }
    else if (reg == 3) {
        CRB = (CRB & 0xC0) | (value & 0x3F);
    }
}
