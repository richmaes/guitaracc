#!/usr/bin/env python3
import serial
import time
import sys
from select_port import select_port

port = select_port(auto_select=True)
if port is None:
    print("No port selected. Exiting.")
    sys.exit(1)

try:
    ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
    print(f"Connected to {port}")
    time.sleep(1)
    ser.reset_input_buffer()
    
    # Send command
    print("Sending: config midi_ch 2")
    ser.write(b'config midi_ch 2\r\n')
    ser.flush()
    
    # Wait and read response
    time.sleep(1.5)
    if ser.in_waiting:
        response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        print("Response:")
        print(response)
    else:
        print("No response")
    
    ser.close()
    print("Done")
except Exception as e:
    print(f'Error: {e}')
    sys.exit(1)
