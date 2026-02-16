#!/usr/bin/env python3
"""
GuitarAcc Basestation UI Test Tool
Connects to the basestation VCOM0 interface and provides an interactive terminal.
"""

import serial
import sys
import threading
import time
import select
from pathlib import Path

# Default serial port for macOS (adjust as needed)
DEFAULT_PORT = "/dev/tty.usbmodem0010501849051"
BAUD_RATE = 115200

class GuitarAccTerminal:
    def __init__(self, port, baudrate=BAUD_RATE):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.running = False
        
    def connect(self):
        """Connect to the serial port"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                xonxoff=False,
                rtscts=True,  # Hardware flow control
                dsrdtr=False
            )
            print(f"Connected to {self.port} at {self.baudrate} baud")
            print("Press Ctrl+C to exit")
            print("-" * 50)
            return True
        except serial.SerialException as e:
            print(f"Error opening serial port: {e}")
            return False
            
    def disconnect(self):
        """Disconnect from the serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("\nDisconnected")
            
    def read_thread(self):
        """Thread to read from serial port and display"""
        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting)
                    sys.stdout.write(data.decode('utf-8', errors='replace'))
                    sys.stdout.flush()
            except Exception as e:
                if self.running:
                    print(f"\nRead error: {e}")
                break
                
    def run(self):
        """Run the interactive terminal"""
        if not self.connect():
            return
            
        self.running = True
        
        # Start read thread
        reader = threading.Thread(target=self.read_thread, daemon=True)
        reader.start()
        
        try:
            # Main input loop
            while self.running:
                # Read from stdin
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    line = sys.stdin.readline()
                    if line:
                        self.ser.write(line.encode('utf-8'))
                        self.ser.flush()
        except KeyboardInterrupt:
            print("\nExiting...")
        finally:
            self.running = False
            self.disconnect()

def run_automated_test(port):
    """Run automated command tests"""
    print("Running automated tests...")
    print("-" * 50)
    
    try:
        ser = serial.Serial(
            port=port,
            baudrate=BAUD_RATE,
            timeout=2,
            rtscts=True
        )
        
        # Wait for any startup messages
        time.sleep(1)
        ser.reset_input_buffer()
        
        commands = [
            ("help", "Testing help command"),
            ("status", "Testing status command"),
            ("echo off", "Testing echo off"),
            ("echo on", "Testing echo on"),
            ("clear", "Testing clear screen"),
        ]
        
        for cmd, desc in commands:
            print(f"\n{desc}: '{cmd}'")
            ser.write(f"{cmd}\r\n".encode('utf-8'))
            ser.flush()
            
            # Read response
            time.sleep(0.5)
            if ser.in_waiting:
                response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
                print("Response:")
                print(response)
            else:
                print("No response received")
                
        ser.close()
        print("\n" + "-" * 50)
        print("Automated tests complete")
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        return False
    
    return True

def list_ports():
    """List available serial ports"""
    import glob
    
    ports = glob.glob('/dev/tty.usbmodem*')
    if not ports:
        ports = glob.glob('/dev/tty.usb*')
    
    if ports:
        print("Available serial ports:")
        for port in ports:
            print(f"  {port}")
    else:
        print("No USB serial ports found")
        
    return ports

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='GuitarAcc Basestation UI Test Tool'
    )
    parser.add_argument(
        '-p', '--port',
        default=DEFAULT_PORT,
        help=f'Serial port (default: {DEFAULT_PORT})'
    )
    parser.add_argument(
        '-b', '--baudrate',
        type=int,
        default=BAUD_RATE,
        help=f'Baud rate (default: {BAUD_RATE})'
    )
    parser.add_argument(
        '-t', '--test',
        action='store_true',
        help='Run automated tests instead of interactive mode'
    )
    parser.add_argument(
        '-l', '--list',
        action='store_true',
        help='List available serial ports'
    )
    
    args = parser.parse_args()
    
    if args.list:
        list_ports()
        return
        
    if args.test:
        success = run_automated_test(args.port)
        sys.exit(0 if success else 1)
    
    # Interactive mode
    terminal = GuitarAccTerminal(args.port, args.baudrate)
    terminal.run()

if __name__ == '__main__':
    main()
