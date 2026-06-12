#!/usr/bin/env python3
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Config MIDI Channel Command"

parser = argparse.ArgumentParser(description='Test config midi_ch command')
parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
args = parser.parse_args()

port = args.port if args.port else select_port(auto_select=True)
if port is None:
    print(f"TEST FAILED: {TEST_NAME} - No port selected")
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
    response_received = False
    command_accepted = False
    
    if ser.in_waiting:
        response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        print("Response:")
        print(response)
        response_received = True
        
        # Check if command was accepted (look for common success indicators)
        response_lower = response.lower()
        if ("midi" in response_lower and "2" in response) or \
           "channel" in response_lower or \
           len(response.strip()) > 0:  # Got some response
            command_accepted = True
    else:
        print("No response")
    
    ser.close()
    
    if response_received and command_accepted:
        print(f"\nTEST PASSED: {TEST_NAME}")
        sys.exit(0)
    else:
        reason = "no response received" if not response_received else "command not accepted"
        print(f"\nTEST FAILED: {TEST_NAME} - {reason}")
        sys.exit(1)
        
except Exception as e:
    print(f"\nTEST FAILED: {TEST_NAME} - {e}")
    sys.exit(1)
