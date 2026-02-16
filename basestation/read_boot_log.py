#!/usr/bin/env python3
import serial
import time

port = '/dev/tty.usbmodem0010501849051'
baudrate = 115200

print(f"Opening {port}...")
with serial.Serial(port, baudrate, timeout=1) as ser:
    time.sleep(0.5)
    
    # Trigger a reset via CLI command
    ser.write(b'kernel reboot cold\n')
    time.sleep(0.1)
    
    # Read for 3 seconds to capture boot
    start = time.time()
    while time.time() - start < 3.0:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            print(data.decode('utf-8', errors='replace'), end='')
        time.sleep(0.1)

print("\n\nBoot log complete")
