#!/usr/bin/env python3
"""
Test MIDI real-time message transmission via priority queue
"""

import serial
import time

PORT = '/dev/tty.usbmodem0010501494421'
BAUDRATE = 115200

def send_command(ser, cmd):
    """Send command and get response"""
    ser.write(f'{cmd}\r\n'.encode())
    time.sleep(0.1)
    response = ser.read(ser.in_waiting)
    return response.decode('utf-8', errors='ignore')

def main():
    print(f"Opening {PORT}...")
    with serial.Serial(PORT, BAUDRATE, timeout=1) as ser:
        # Send enter to get prompt
        ser.write(b'\r\n')
        time.sleep(0.1)
        ser.read(ser.in_waiting)
        
        print("\nTesting MIDI real-time transmission via priority queue\n")
        print("=" * 60)
        
        # Test sending MIDI Start (0xFA)
        print("\n1. Sending MIDI Start (0xFA)...")
        response = send_command(ser, 'midi send_rt 0xFA')
        print(response)
        
        # Test sending MIDI Clock (0xF8)
        print("\n2. Sending MIDI Clock (0xF8)...")
        response = send_command(ser, 'midi send_rt 0xF8')
        print(response)
        
        # Test sending MIDI Stop (0xFC)
        print("\n3. Sending MIDI Stop (0xFC)...")
        response = send_command(ser, 'midi send_rt 0xFC')
        print(response)
        
        # Test sending multiple clocks
        print("\n4. Sending 10 MIDI Clock messages...")
        for i in range(10):
            response = send_command(ser, 'midi send_rt 0xF8')
            if i == 0:
                print(response)
            else:
                print(".", end="", flush=True)
        print("\nDone!")
        
        # Test error handling - invalid byte
        print("\n5. Testing invalid byte (should fail)...")
        response = send_command(ser, 'midi send_rt 0x90')
        print(response)
        
        print("\n" + "=" * 60)
        print("\nAll tests completed!")
        print("\nYou can verify these messages were sent by checking")
        print("your MIDI monitor or input device.")

if __name__ == '__main__':
    main()
