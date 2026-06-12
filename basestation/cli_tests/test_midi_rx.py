#!/usr/bin/env python3
"""
Test MIDI RX statistics command
"""

import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "MIDI RX Statistics Test"

def main():
    parser = argparse.ArgumentParser(description='Test MIDI RX statistics command')
    parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
    args = parser.parse_args()
    
    port = args.port if args.port else select_port(auto_select=True)
    if port is None:
        print(f"TEST FAILED: {TEST_NAME} - No port selected")
        sys.exit(1)
    
    try:
        print(f"Opening {port}...")
        with serial.Serial(port, 115200, timeout=1) as ser:
            # Send enter to get prompt
            ser.write(b'\r\n')
            time.sleep(0.1)
            ser.read(ser.in_waiting)
            
            # Send midi rx_stats command
            print("\nSending 'midi rx_stats' command...")
            ser.write(b'midi rx_stats\r\n')
            time.sleep(0.2)
            
            # Read response
            response = ser.read(ser.in_waiting)
            response_text = response.decode('utf-8', errors='ignore')
            print("\nResponse:")
            print(response_text)
            
            # Check if we got a valid response
            if not response_text or len(response_text.strip()) == 0:
                print(f"\nTEST FAILED: {TEST_NAME} - No response to midi rx_stats command")
                sys.exit(1)
            
            # Wait a few seconds and check again
            print("\nWaiting 5 seconds...")
            time.sleep(5)
            
            print("\nSending 'midi rx_stats' command again...")
            ser.write(b'midi rx_stats\r\n')
            time.sleep(0.2)
            
            response2 = ser.read(ser.in_waiting)
            response2_text = response2.decode('utf-8', errors='ignore')
            print("\nResponse:")
            print(response2_text)
            
            if not response2_text or len(response2_text.strip()) == 0:
                print(f"\nTEST FAILED: {TEST_NAME} - No response on second attempt")
                sys.exit(1)
            
            print(f"\nTEST PASSED: {TEST_NAME}")
            sys.exit(0)
            
    except Exception as e:
        print(f"\nTEST FAILED: {TEST_NAME} - {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
