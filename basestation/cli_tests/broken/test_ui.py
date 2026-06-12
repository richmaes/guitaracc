#!/usr/bin/env python3
"""
GuitarAcc Basestation UI Test Tool
Connects to the basestation VCOM0 interface and provides an interactive terminal.
"""

import serial
import sys
import threading
import time
import tty
import termios
from pathlib import Path
from select_port import select_port

# Baud rate for basestation UART
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
                else:
                    time.sleep(0.01)  # Small delay to avoid busy loop
            except Exception as e:
                if self.running:
                    print(f"\nRead error: {e}")
                break
    
    def input_thread(self):
        """Thread to read from stdin and send to serial (raw mode for special keys)"""
        old_settings = None
        try:
            # Save old terminal settings and switch to raw mode
            old_settings = termios.tcgetattr(sys.stdin)
            tty.setraw(sys.stdin.fileno())
            
            while self.running:
                try:
                    # Read one character at a time
                    char = sys.stdin.read(1)
                    if char and self.ser and self.ser.is_open:
                        # Handle Ctrl+C (0x03)
                        if ord(char) == 3:
                            self.running = False
                            break
                        # Send character to serial
                        self.ser.write(char.encode('utf-8') if isinstance(char, str) else char)
                        self.ser.flush()
                except Exception as e:
                    if self.running:
                        print(f"\nInput error: {e}")
                    break
        finally:
            # Restore terminal settings
            if old_settings:
                termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
                
    def run(self):
        """Run the interactive terminal"""
        if not self.connect():
            return
            
        self.running = True
        
        # Start read thread
        reader = threading.Thread(target=self.read_thread, daemon=True)
        reader.start()
        
        # Start input thread
        writer = threading.Thread(target=self.input_thread, daemon=True)
        writer.start()
        
        try:
            # Wait for Ctrl+C
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nExiting...")
        finally:
            self.running = False
            time.sleep(0.2)  # Give threads time to exit
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
            ("config show", "Testing config show"),
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

def write_factory_defaults(port):
    """Write factory default configuration to device"""
    print("Writing factory default configuration...")
    print("-" * 50)
    print("WARNING: This will overwrite the DEFAULT area in QSPI flash!")
    print("This should only be done during manufacturing/setup.")
    print("")
    print("NOTE: Firmware must be built with CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y")
    
    confirm = input("Type 'YES' to continue: ")
    if confirm != "YES":
        print("Aborted.")
        return False
    
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
        
        print("\nStep 1: Unlocking DEFAULT area...")
        ser.write(b"config unlock_default\r\n")
        ser.flush()
        time.sleep(0.5)
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            print(response)
        
        print("\nStep 2: Writing factory defaults...")
        ser.write(b"config write_default\r\n")
        ser.flush()
        
        # Read response
        time.sleep(1)
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            print("Response:")
            print(response)
        else:
            print("No response received")
        
        ser.close()
        print("\n" + "-" * 50)
        print("Factory defaults written successfully")
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        return False
    
    return True


def erase_config_storage(port):
    """Erase all configuration storage areas (testing only)"""
    print("Erasing configuration storage...")
    print("-" * 50)
    print("WARNING: This will erase all configuration data!")
    print("The device will boot with hardcoded defaults.")
    print("This tests that uninitialized config doesn't lock up the system.")
    
    confirm = input("Type 'ERASE' to continue: ")
    if confirm != "ERASE":
        print("Aborted.")
        return False
    
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
        
        print("\nErasing all configuration areas...")
        ser.write(b"config erase_all\r\n")
        ser.flush()
        
        # Read response
        time.sleep(1.5)
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            print("Response:")
            print(response)
        else:
            print("No response received")
        
        ser.close()
        print("\n" + "-" * 50)
        print("Configuration storage erased")
        print("Please reboot the device to test initialization with no config")
        
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
        default=None,
        help='Serial port (if not specified, will prompt for selection)'
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
    parser.add_argument(
        '-w', '--write-defaults',
        action='store_true',
        help='Write factory default configuration (WARNING: manufacturing only!)'
    )
    parser.add_argument(
        '-e', '--erase-config',
        action='store_true',
        help='Erase all configuration storage (WARNING: testing only!)'
    )
    
    args = parser.parse_args()
    
    if args.list:
        list_ports()
        return
    
    # Select port if not specified
    if args.port is None:
        args.port = select_port(auto_select=True)
        if args.port is None:
            print("No port selected. Exiting.")
            sys.exit(1)
        
    if args.erase_config:
        success = erase_config_storage(args.port)
        sys.exit(0 if success else 1)
        
    if args.write_defaults:
        success = write_factory_defaults(args.port)
        sys.exit(0 if success else 1)
        
    if args.test:
        success = run_automated_test(args.port)
        sys.exit(0 if success else 1)
    
    # Interactive mode
    terminal = GuitarAccTerminal(args.port, args.baudrate)
    terminal.run()

if __name__ == '__main__':
    main()
