# Apple 1 Emulator for ESP32 CYD (Cheap Yellow Display)

A faithful Apple 1 computer emulator running on the ESP32 with a 2.8" ILI9341 TFT display (commonly known as the "Cheap Yellow Display" or CYD). Features a complete 6502 CPU emulation, WOZ Monitor, and Integer BASIC.

## Features

- **Full 6502 CPU emulation** — all documented opcodes
- **WOZ Monitor** — the original Apple 1 system monitor ROM
- **Integer BASIC** — Woz's 4KB BASIC interpreter
- **40×24 character display** — green phosphor aesthetic on the TFT
- **Blinking cursor** — authentic `@` block cursor
- **Serial terminal input** — interact via any serial terminal at 115200 baud
- **Debug tracing** — toggle CPU state tracing with `~` (tilde)
- **Program uploader** — Python script for pasting large hex dumps

## Hardware

| Component | Details |
|-----------|---------|
| Board | ESP32-D0WD-V3 (Dual Core, 240MHz) — "Cheap Yellow Display" |
| Display | 2.8" ILI9341 320×240 TFT (SPI) |
| Touch | XPT2046 (available but not used by emulator) |

## Software Versions

| Dependency | Version |
|------------|---------|
| ESP32 Arduino Core | **2.0.17** (`esp32:esp32`) |
| TFT_eSPI | **2.5.43** by Bodmer |
| Arduino CLI | Any recent version |
| Python (for uploader) | 3.x with `pyserial` |

## TFT_eSPI Configuration (`User_Setup.h`)

The CYD uses **HSPI** with specific pin mappings. The following settings must be configured in your `TFT_eSPI/User_Setup.h`:

### Display Driver
```cpp
#define ILI9341_2_DRIVER     // Alternative ILI9341 driver for CYD
#define TFT_INVERSION_ON      // Required — colours are inverted without this
```

### Pin Definitions (ESP32 CYD)
```cpp
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15           // Chip select
#define TFT_DC    2           // Data/Command
#define TFT_RST  -1           // Reset tied to ESP32 RST
#define TFT_BL   21           // Backlight control pin
#define TFT_BACKLIGHT_ON HIGH
```

### Touch (optional)
```cpp
#define TOUCH_CS 33            // XPT2046 chip select
```

### SPI Configuration
```cpp
#define SPI_FREQUENCY       55000000   // 55MHz SPI clock
#define SPI_READ_FREQUENCY  20000000   // 20MHz for reads
#define SPI_TOUCH_FREQUENCY  2500000   // 2.5MHz for touch
#define USE_HSPI_PORT                  // CYD uses HSPI, not default VSPI
```

### Fonts (all enabled)
```cpp
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
```

> **Important**: The CYD `User_Setup.h` originates from [Random Nerd Tutorials](https://RandomNerdTutorials.com/cyd/). Standard configurations from the internet will likely **not work** with this board.

## Building & Uploading

### Prerequisites
```bash
# Install ESP32 core
arduino-cli core install esp32:esp32

# Install TFT_eSPI library
arduino-cli lib install "TFT_eSPI"

# Then edit User_Setup.h as described above
```

### Compile
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 Apple1_CYD.ino
```

### Upload
```bash
arduino-cli upload --fqbn esp32:esp32:esp32 -p COM4 Apple1_CYD.ino
```

### Connect
```bash
arduino-cli monitor -p COM4 -c baudrate=115200
```

## Usage

### WOZ Monitor
The emulator boots directly into the WOZ Monitor (the `\` prompt):

```
\
```

- Enter hex addresses: `FF00` (examine memory)
- Enter hex data: `0300: A9 01 8D 12 D0 60` (write to memory)
- Run code: `0300R`
- Enter BASIC: `E000R`

### Integer BASIC
```
E000R
>10 PRINT "HELLO WORLD"
>20 GOTO 10
>RUN
```

### Debug Tracing
Send `~` (tilde) from the terminal to toggle CPU debug output:
- Periodic CPU state dumps (PC, A, X, Y, SP, flags)
- Illegal opcode detection
- BRK instruction alerts
- Low stack warnings
- Unmapped memory access warnings

## Uploading Large Programs (`send_to_apple1.py`)

When pasting large hex dumps directly into the serial terminal, the ESP32's serial buffer can overflow. Use the included Python uploader:

### Install dependency
```bash
pip install pyserial
```

### Send a file
```bash
python send_to_apple1.py program.hex
```

### Interactive paste mode
```bash
python send_to_apple1.py
```
Paste your program, then press Enter on an empty line.

### Options
```
  -p PORT     COM port (default: COM4)
  -b BAUD     Baud rate (default: 115200)
  -cd DELAY   Character delay in seconds (default: 0.005)
  -ld DELAY   Line delay in seconds (default: 0.15)
  --no-terminal  Exit after sending
```

### Example: Load a game
```bash
python send_to_apple1.py -p COM4 lunar_lander.hex
```

The script sends one character at a time with pacing delays, then drops into an interactive terminal session.

## Memory Map

| Address Range | Size | Contents |
|--------------|------|----------|
| `$0000–$1FFF` | 8 KB | RAM |
| `$D010–$D013` | 4 | PIA (6821) — Keyboard & Display I/O |
| `$E000–$EFFF` | 4 KB | Integer BASIC ROM |
| `$FF00–$FFFF` | 256 B | WOZ Monitor ROM |

## File Structure

```
Apple1_CYD/
├── Apple1_CYD.ino      # Main sketch — setup, loop, serial I/O
├── cpu6502.h/.cpp       # Complete 6502 CPU emulation
├── memory.h/.cpp        # Memory bus — RAM, ROM, PIA routing
├── pia.h/.cpp           # 6821 PIA emulation (keyboard + display)
├── display.h/.cpp       # 40×24 character display on TFT
├── roms.h               # BASIC ROM, WOZ Monitor ROM, character ROM
├── send_to_apple1.py    # Python serial uploader for large programs
└── README.md            # This file
```

## Known Issues & Notes

- **CR+LF terminals**: The emulator filters out LF (`0x0A`) characters. Terminals sending CR+LF on Enter work correctly. Without this filter, the extra LF would be interpreted as a break character by BASIC, causing loops to terminate after ~4–6 iterations.
- **Serial buffer**: The ESP32 hardware serial buffer is limited (~256 bytes). For large program uploads, use the `send_to_apple1.py` script.
- **Display rotation**: Set to `tft.setRotation(1)` for landscape orientation matching the CYD's physical layout.

## Credits

- **Steve Wozniak** — Original Apple 1 hardware, WOZ Monitor, and Integer BASIC
- **Bodmer** — TFT_eSPI library
- **Random Nerd Tutorials** — CYD User_Setup.h configuration
