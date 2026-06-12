#!/usr/bin/env python3
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Configuration Persistence Test"

parser = argparse.ArgumentParser(description='Test configuration persistence')
parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
args = parser.parse_args()

port = args.port if args.port else select_port(auto_select=True)
if port is None:
    print(f"TEST FAILED: {TEST_NAME} - No port selected")
    sys.exit(1)

baudrate = 115200

try:
    print(f"Opening {port}...")
    with serial.Serial(port, baudrate, timeout=1) as ser:
        time.sleep(0.5)
        ser.read_all()  # Clear buffer
        
        # Show current config
        print("\n=== Current Configuration ===")
        ser.write(b'config show\n')
        time.sleep(0.3)
        response1 = ser.read_all().decode('utf-8', errors='replace')
        print(response1)
        
        if not response1 or len(response1.strip()) == 0:
            print(f"\nTEST FAILED: {TEST_NAME} - No response to config show")
            sys.exit(1)
        
        # Change MIDI channel to 5
        print("\n=== Setting MIDI channel to 5 ===")
        ser.write(b'config midi_ch 5\n')
        time.sleep(0.3)
        response2 = ser.read_all().decode('utf-8', errors='replace')
        print(response2)
        
        # Save configuration
        print("\n=== Saving configuration ===")
        ser.write(b'config save\n')
        time.sleep(0.5)
        response_save = ser.read_all().decode('utf-8', errors='replace')
        print(response_save)
        
        # Show updated config
        print("\n=== Updated Configuration ===")
        ser.write(b'config show\n')
        time.sleep(0.3)
        response3 = ser.read_all().decode('utf-8', errors='replace')
        print(response3)
        
        # Verify MIDI channel was changed
        if "MIDI channel: 5" in response3 or "midi_ch: 5" in response3 or "channel 5" in response3.lower():
            print(f"\n✓ MIDI channel successfully changed to 5")
            print(f"\nTEST PASSED: {TEST_NAME}")
            print("\nNote: Power cycle the board and run 'config show' to verify persistence across reboots")
            sys.exit(0)
        else:
            print(f"\nTEST FAILED: {TEST_NAME} - MIDI channel not set to 5")
            sys.exit(1)
            
except Exception as e:
    print(f"\nTEST FAILED: {TEST_NAME} - {e}")
    sys.exit(1)
