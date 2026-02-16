#!/usr/bin/env python3
"""
Quick test to check MIDI RX statistics
"""

import serial
import time

PORT = '/dev/tty.usbmodem0010501494421'
BAUDRATE = 115200

def main():
    print(f"Opening {PORT}...")
    with serial.Serial(PORT, BAUDRATE, timeout=1) as ser:
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
        print("\nResponse:")
        print(response.decode('utf-8', errors='ignore'))
        
        # Wait a few seconds and check again
        print("\nWaiting 5 seconds...")
        time.sleep(5)
        
        print("\nSending 'midi rx_stats' command again...")
        ser.write(b'midi rx_stats\r\n')
        time.sleep(0.2)
        
        response = ser.read(ser.in_waiting)
        print("\nResponse:")
        print(response.decode('utf-8', errors='ignore'))

if __name__ == '__main__':
    main()
