/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        display.h
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Header for the Apple I display emulation. Defines
 *              the 40x24 character terminal with green phosphor
 *              aesthetics, scrolling, cursor blinking, and
 *              character rendering via the TFT_eSPI library.
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

#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include <stdint.h>

class Apple1Display {
public:
    Apple1Display(TFT_eSPI* tft_ptr);
    
    // Initialize screen, clear it
    void init();
    
    // Write a character from the Apple 1 PIA
    void writeChar(uint8_t c);
    
    // Clear display buffer and screen
    void clear();
    
    // Renders any dirty characters to the screen
    void refresh();

private:
    TFT_eSPI* tft;
    uint8_t buffer[24][40];
    bool dirty[24][40];
    
    int cursor_x;
    int cursor_y;
    
    unsigned long last_blink_time;
    bool cursor_state;
    int last_cursor_x;
    int last_cursor_y;
    
    // Display coordinates / padding
    // Mapped for 320x240 landscape matrix (40*8 x 24*10 = 320x240)
    const int MARGIN_X = 0;
    const int MARGIN_Y = 0;
    const int CHAR_W = 8;
    const int CHAR_H = 10;
    
    void scroll();
    void drawChar(int row, int col, uint8_t c);
    void handleSpecial(uint8_t c);
};

#endif // DISPLAY_H
