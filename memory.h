/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        memory.h
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Header for the Apple I memory bus. Declares the
 *              address decoder that routes reads and writes to
 *              8KB RAM, PIA I/O ($D010-$D013), Integer BASIC ROM
 *              ($E000-$EFFF), and WOZ Monitor ROM ($FF00-$FFFF).
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "pia.h"

// Standard Apple 1 Configuration: 8KB RAM
#define RAM_SIZE 8192

class Apple1Memory {
public:
    Apple1Memory(Apple1PIA* pia_ptr);
    
    void reset();
    
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    
    bool debug;
    
    uint16_t readWord(uint16_t address);
    
private:
    Apple1PIA* pia;
    uint8_t ram[RAM_SIZE];
};

#endif // MEMORY_H
