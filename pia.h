/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        pia.h
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Header for the MC6821 PIA (Peripheral Interface
 *              Adapter) emulation. Handles keyboard input via
 *              Port A ($D010-$D011) and display output via
 *              Port B ($D012-$D013), mirroring the original
 *              Apple I I/O architecture.
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

#ifndef PIA_H
#define PIA_H

#include <stdint.h>

class Apple1Display; // Forward declaration

class Apple1PIA {
public:
    Apple1PIA();
    
    void setDisplay(Apple1Display* disp);
    
    void reset();
    
    // Address is expected to be 0-3 (0xD010 - 0xD013 mapped to 0-3)
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    
    // Called by Serial event to push a key
    void keyPress(uint8_t key);
    bool isKeyReady() const { return key_ready; }
    
private:
    Apple1Display* display;
    
    // Registers
    uint8_t DDRA, CRA, ORA;
    uint8_t DDRB, CRB, ORB;
    
    // Status
    uint8_t last_key;
    bool key_ready;
    bool display_ready;
};

#endif // PIA_H
