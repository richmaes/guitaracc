#!/usr/bin/env python3
import serial
import time

port = '/dev/tty.usbmodem0010501849051'
baudrate = 115200

print(f"Opening {port}...")
with serial.Serial(port, baudrate, timeout=1) as ser:
    time.sleep(0.5)
    ser.read_all()  # Clear buffer
    
    # Show current config
    print("\n=== Current Configuration ===")
    ser.write(b'config show\n')
    time.sleep(0.3)
    response = ser.read_all().decode('utf-8', errors='replace')
    print(response)
    
    # Change MIDI channel to 5
    print("\n=== Setting MIDI channel to 5 ===")
    ser.write(b'config midi_ch 5\n')
    time.sleep(0.3)
    response = ser.read_all().decode('utf-8', errors='replace')
    print(response)
    
    # Show updated config
    print("\n=== Updated Configuration ===")
    ser.write(b'config show\n')
    time.sleep(0.3)
    response = ser.read_all().decode('utf-8', errors='replace')
    print(response)

print("\n*** Please power cycle the board and run this script again to verify persistence ***")
