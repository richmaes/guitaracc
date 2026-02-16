#!/usr/bin/env python3
import serial
import time
from select_port import select_port

port = select_port(auto_select=True)
if not port:
    exit(1)

print(f"Opening {port}...")
ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
time.sleep(1)

print("Sending Enter to get prompt...")
ser.write(b'\r\n')
time.sleep(0.5)

print("Reading response...")
if ser.in_waiting:
    data = ser.read(ser.in_waiting)
    print(f"Got {len(data)} bytes:")
    print(data.decode('utf-8', errors='replace'))
else:
    print("No data received")

print("\nSending 'status' command...")
ser.write(b'status\r\n')
time.sleep(0.5)

if ser.in_waiting:
    data = ser.read(ser.in_waiting)
    print("Response:")
    print(data.decode('utf-8', errors='replace'))
else:
    print("No response")

ser.close()
