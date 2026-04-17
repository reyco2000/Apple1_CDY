#!/usr/bin/env python3
"""
Apple 1 Serial Uploader
Sends large programs line-by-line to the Apple 1 emulator via COM port.
Handles pacing to avoid overflowing the ESP32 serial buffer.

Usage:
  python send_to_apple1.py                     # Interactive mode
  python send_to_apple1.py program.txt         # Send a file
  python send_to_apple1.py -p COM5 program.txt # Specify port

After sending, drops into an interactive terminal session.
Press Ctrl+C to exit.
"""

import serial
import sys
import time
import argparse
import threading
import os

# Defaults
DEFAULT_PORT = "COM4"
DEFAULT_BAUD = 115200
CHAR_DELAY   = 0.005   # 5ms between characters
LINE_DELAY   = 0.15    # 150ms between lines (WOZ Monitor needs time to process)
PROMPT_CHAR  = "\\"    # WOZ Monitor prompt


def read_serial_output(ser, stop_event):
    """Background thread: continuously print data received from the Apple 1."""
    while not stop_event.is_set():
        try:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                text = data.decode('ascii', errors='replace')
                # Print without adding extra newlines
                sys.stdout.write(text)
                sys.stdout.flush()
            else:
                time.sleep(0.01)
        except Exception:
            break


def send_line(ser, line, line_num=0, total=0, char_delay=0.005, line_delay=0.15):
    """Send a single line to the Apple 1 character by character, ending with CR."""
    line = line.rstrip('\r\n')
    if not line:
        return
    
    prefix = f"[{line_num}/{total}] " if total > 0 else ""
    # Show progress on stderr so it doesn't mix with Apple 1 output
    sys.stderr.write(f"\r{prefix}Sending: {line[:60]}{'...' if len(line)>60 else ''}    ")
    sys.stderr.flush()
    
    for ch in line:
        ser.write(ch.encode('ascii'))
        time.sleep(char_delay)
    
    # Send CR (Apple 1 line terminator)
    ser.write(b'\r')
    time.sleep(line_delay)


def send_program(ser, lines, char_delay=0.005, line_delay=0.15):
    """Send a multi-line program to the Apple 1."""
    # Filter out empty lines and comments (lines starting with ;)
    program_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped and not stripped.startswith(';'):
            program_lines.append(stripped)
    
    if not program_lines:
        sys.stderr.write("No lines to send.\n")
        return
    
    total = len(program_lines)
    sys.stderr.write(f"\n{'='*50}\n")
    sys.stderr.write(f"  Sending {total} lines to Apple 1\n")
    sys.stderr.write(f"  Char delay: {char_delay*1000:.0f}ms  Line delay: {line_delay*1000:.0f}ms\n")
    sys.stderr.write(f"{'='*50}\n\n")
    
    for i, line in enumerate(program_lines, 1):
        send_line(ser, line, i, total, char_delay, line_delay)
    
    sys.stderr.write(f"\r{'='*50}\n")
    sys.stderr.write(f"  Done! Sent {total} lines.\n")
    sys.stderr.write(f"{'='*50}\n\n")


def interactive_input():
    """Read multi-line input from the user. Empty line or Ctrl+D to finish."""
    sys.stderr.write("\n" + "="*50 + "\n")
    sys.stderr.write("  Paste your program below.\n")
    sys.stderr.write("  Press Enter on an empty line when done.\n")
    sys.stderr.write("  (Or press Ctrl+Z on Windows / Ctrl+D on Linux)\n")
    sys.stderr.write("="*50 + "\n\n")
    
    lines = []
    try:
        while True:
            line = input()
            if line.strip() == '':
                if lines:  # Only stop on empty line if we already have content
                    break
                continue
            lines.append(line)
    except EOFError:
        pass
    
    return lines


def interactive_terminal(ser, stop_event):
    """Simple interactive terminal after sending the program."""
    sys.stderr.write("\n--- Interactive terminal (Ctrl+C to exit) ---\n\n")
    try:
        while not stop_event.is_set():
            if sys.stdin.readable():
                try:
                    ch = sys.stdin.read(1)
                    if ch:
                        if ch == '\n':
                            ser.write(b'\r')  # Convert Enter to CR
                        else:
                            ser.write(ch.encode('ascii', errors='ignore'))
                        time.sleep(CHAR_DELAY)
                except Exception:
                    break
    except KeyboardInterrupt:
        pass


def main():
    parser = argparse.ArgumentParser(
        description="Apple 1 Serial Uploader — send programs to the Apple 1 emulator"
    )
    parser.add_argument("file", nargs="?", help="Text file to send (optional, interactive if omitted)")
    parser.add_argument("-p", "--port", default=DEFAULT_PORT, help=f"COM port (default: {DEFAULT_PORT})")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("-cd", "--char-delay", type=float, default=CHAR_DELAY,
                        help=f"Delay between characters in seconds (default: {CHAR_DELAY})")
    parser.add_argument("-ld", "--line-delay", type=float, default=LINE_DELAY,
                        help=f"Delay between lines in seconds (default: {LINE_DELAY})")
    parser.add_argument("--no-terminal", action="store_true",
                        help="Exit after sending instead of entering interactive mode")
    
    args = parser.parse_args()
    
    char_delay = args.char_delay
    line_delay = args.line_delay
    
    # Open serial port
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        sys.stderr.write(f"Connected to {args.port} at {args.baud} baud\n")
    except serial.SerialException as e:
        sys.stderr.write(f"Error opening {args.port}: {e}\n")
        sys.stderr.write("Make sure no other program (like arduino-cli monitor) is using the port.\n")
        sys.exit(1)
    
    # Start background reader thread
    stop_event = threading.Event()
    reader = threading.Thread(target=read_serial_output, args=(ser, stop_event), daemon=True)
    reader.start()
    
    # Give a moment to see any boot messages
    time.sleep(0.5)
    
    try:
        # Get lines to send
        if args.file:
            if not os.path.exists(args.file):
                sys.stderr.write(f"File not found: {args.file}\n")
                sys.exit(1)
            with open(args.file, 'r') as f:
                lines = f.readlines()
            sys.stderr.write(f"Loaded {len(lines)} lines from {args.file}\n")
        else:
            lines = interactive_input()
        
        # Send the program
        if lines:
            send_program(ser, lines, char_delay, line_delay)
        
        # Interactive terminal
        if not args.no_terminal:
            interactive_terminal(ser, stop_event)
    
    except KeyboardInterrupt:
        sys.stderr.write("\n\nExiting...\n")
    finally:
        stop_event.set()
        ser.close()
        sys.stderr.write("Port closed.\n")


if __name__ == "__main__":
    main()
