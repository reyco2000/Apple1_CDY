/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        display.cpp
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: 40x24 character terminal display for the Apple I.
 *              Renders characters from the Apple I character ROM
 *              onto the ILI9341 TFT using pixel-level drawing.
 *              Implements screen scrolling, carriage return/line
 *              feed handling, and a blinking block cursor.
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

#include "display.h"
#include "roms.h" // For CHARMAP

#define COLOR_GREEN 0x07E0 // Basic 16-bit brightest green
#define COLOR_BG    0x0120 // Dark green background for phosphor glow effect

Apple1Display::Apple1Display(TFT_eSPI* tft_ptr) : tft(tft_ptr) {
    cursor_x = 0;
    cursor_y = 0;
    last_blink_time = 0;
    cursor_state = false;
    last_cursor_x = -1;
    last_cursor_y = -1;
}

void Apple1Display::init() {
    clear();
    tft->fillScreen(COLOR_BG);
}

void Apple1Display::clear() {
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 40; x++) {
            buffer[y][x] = 0x20; // Space
            dirty[y][x] = true;
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void Apple1Display::scroll() {
    // Shift buffer up by 1 row
    for (int y = 0; y < 23; y++) {
        for (int x = 0; x < 40; x++) {
            buffer[y][x] = buffer[y+1][x];
            dirty[y][x] = true;
        }
    }
    // Clear last row
    for (int x = 0; x < 40; x++) {
        buffer[23][x] = 0x20;
        dirty[23][x] = true;
    }
    cursor_y = 23;
}

void Apple1Display::handleSpecial(uint8_t c) {
    if (c == 0x0D) { // Carriage return
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= 24) {
            scroll();
        }
    } else if (c == 0x08 || c == 0x7F || c == '_') { // Backspace / Rubout
        if (cursor_x > 0) {
            cursor_x--;
            buffer[cursor_y][cursor_x] = 0x20;
            dirty[cursor_y][cursor_x] = true;
        }
    }
}

void Apple1Display::writeChar(uint8_t c) {
    c &= 0x7F; // Apple 1 is 7-bit ASCII
    
    if (c == 0x0D || c == 0x08 || c == 0x7F || c == '_') {
        handleSpecial(c);
        return;
    }
    
    if (c >= 0x20 && c < 0x7F) {
        if (c >= 0x61 && c <= 0x7A) {
            c -= 0x20; // Convert to uppercase
        }
        
        buffer[cursor_y][cursor_x] = c;
        dirty[cursor_y][cursor_x] = true;
        
        cursor_x++;
        if (cursor_x >= 40) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= 24) {
                scroll();
            }
        }
    }
}

void Apple1Display::drawChar(int row, int col, uint8_t c) {
    // Determine char offset in the 2513 ROM
    int charIndex = c & 0x7F;
    int romOffset = charIndex * 8;
    
    int screenX = MARGIN_X + (col * CHAR_W);
    int screenY = MARGIN_Y + (row * CHAR_H);
    
    // Fill the 8x10 background cell
    tft->fillRect(screenX, screenY, CHAR_W, CHAR_H, COLOR_BG);
    
    // Read 8 rows of pixels for this character
    for (int r = 0; r < 8; r++) {
        uint8_t pixels = pgm_read_byte(&CHARMAP[romOffset + r]);
        
        for (int p = 0; p < 7; p++) {
            bool pixelOn = (pixels >> p) & 1;
            if (pixelOn) {
                tft->drawPixel(screenX + p + 1, screenY + r + 1, COLOR_GREEN);
            }
        }
    }
}

void Apple1Display::refresh() {
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 40; x++) {
            if (dirty[y][x]) {
                drawChar(y, x, buffer[y][x]);
                dirty[y][x] = false;
            }
        }
    }
    
    // Hardware Cursor Overlay 
    unsigned long current_time = millis();
    if (current_time - last_blink_time >= 500) {
        cursor_state = !cursor_state;
        last_blink_time = current_time;
        
        if (cursor_state) {
            drawChar(cursor_y, cursor_x, '@'); // Draw original Apple 1 cursor block
        } else {
            drawChar(cursor_y, cursor_x, buffer[cursor_y][cursor_x]); // Erase by restoring underlying char
        }
    } else if (last_cursor_x != cursor_x || last_cursor_y != cursor_y) {
        // Cursor moved before blink toggle. Immediately clear old and draw new!
        if (last_cursor_x != -1 && last_cursor_y != -1) {
            drawChar(last_cursor_y, last_cursor_x, buffer[last_cursor_y][last_cursor_x]);
        }
        cursor_state = true;
        last_blink_time = current_time;
        drawChar(cursor_y, cursor_x, '@');
        
        last_cursor_x = cursor_x;
        last_cursor_y = cursor_y;
    }
}
