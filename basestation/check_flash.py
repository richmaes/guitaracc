#!/usr/bin/env python3
import serial
import time

port = '/dev/tty.usbmodem0010501849051'
baudrate = 115200

print(f"Opening {port}...")
with serial.Serial(port, baudrate, timeout=1) as ser:
    time.sleep(0.5)
    
    # Clear buffer
    ser.read_all()
    
    # Try config show to see current state
    print("Sending: config show")
    ser.write(b'config show\n')
    time.sleep(0.3)
    
    data = ser.read_all()
    print(data.decode('utf-8', errors='replace'))

