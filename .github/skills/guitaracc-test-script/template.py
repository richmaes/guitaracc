#!/usr/bin/env python3
"""
[Brief description of what this test validates]
"""
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Your Test Name Here"

def main():
    parser = argparse.ArgumentParser(description='Brief test description')
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
        ser.reset_input_buffer()
        
        # ===== Test Logic Starts Here =====
        
        print("Sending command...")
        ser.write(b'your_command\r\n')
        ser.flush()
        time.sleep(0.5)
        
        # Validate Response
        test_passed = False
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            print("Response:")
            print(response)
            
            # TODO: Add validation logic here
            if "expected_output" in response:
                test_passed = True
        else:
            print("No response received")
        
        # ===== Test Logic Ends Here =====
        
        ser.close()
        
        # Report Result
        if test_passed:
            print(f"\nTEST PASSED: {TEST_NAME}")
            sys.exit(0)
        else:
            print(f"\nTEST FAILED: {TEST_NAME} - [specific reason]")
            sys.exit(1)
            
    except Exception as e:
        print(f"\nTEST FAILED: {TEST_NAME} - {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
