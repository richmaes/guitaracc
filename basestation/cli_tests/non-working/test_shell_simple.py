#!/usr/bin/env python3
import serial
import time

port = '/dev/tty.usbmodem0010501494421'

print(f"Opening {port}...")
try:
    ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
    time.sleep(1)
    
    print("Sending Enter to get prompt...")
    ser.write(b'\r\n')
    time.sleep(0.5)
    
    print("Reading response...")
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        print(f"Got {len(data)} bytes:")
        print(repr(data))
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
        print("No response to status command")
    
    ser.close()
    print("Done")
except Exception as e:
    print(f"Error: {e}")
