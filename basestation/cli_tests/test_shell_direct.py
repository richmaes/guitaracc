#!/usr/bin/env python3
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Shell Direct Connectivity Test"

parser = argparse.ArgumentParser(description='Test direct shell access with raw output')
parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
args = parser.parse_args()

port = args.port if args.port else select_port(auto_select=True)
if port is None:
    print(f"TEST FAILED: {TEST_NAME} - No port selected")
    sys.exit(1)

try:
    print(f"Opening {port}...")
    ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
    time.sleep(1)
    
    print("Sending Enter to get prompt...")
    ser.write(b'\r\n')
    time.sleep(0.5)
    
    print("Reading response...")
    prompt_received = False
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        print(f"Got {len(data)} bytes:")
        print(repr(data))
        response_text = data.decode('utf-8', errors='replace')
        print(response_text)
        if len(data) > 0:
            prompt_received = True
    else:
        print("No data received")
    
    print("\nSending 'status' command...")
    ser.write(b'status\r\n')
    time.sleep(0.5)
    
    status_received = False
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        response = data.decode('utf-8', errors='replace')
        print("Response:")
        print(response)
        if len(data) > 0:
            status_received = True
    else:
        print("No response to status command")
    
    ser.close()
    
    if prompt_received and status_received:
        print(f"\nTEST PASSED: {TEST_NAME}")
        sys.exit(0)
    else:
        reason = []
        if not prompt_received:
            reason.append("no prompt response")
        if not status_received:
            reason.append("no status response")
        print(f"\nTEST FAILED: {TEST_NAME} - {', '.join(reason)}")
        sys.exit(1)
        
except Exception as e:
    print(f"\nTEST FAILED: {TEST_NAME} - {e}")
    sys.exit(1)
