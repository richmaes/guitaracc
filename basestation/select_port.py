#!/usr/bin/env python3
"""
Serial Port Selection Utility
Finds and allows selection of USB modem ports for GuitarAcc testing
"""

import glob
import sys

def find_usb_modem_ports():
    """Find all /dev/tty.usbmodem* ports"""
    ports = glob.glob('/dev/tty.usbmodem*')
    return sorted(ports)

def select_port(auto_select=False):
    """
    Present a menu to select a USB modem port
    
    Args:
        auto_select: If True and only one port found, select it automatically
        
    Returns:
        Selected port path as string, or None if cancelled/no ports
    """
    ports = find_usb_modem_ports()
    
    if not ports:
        print("ERROR: No USB modem ports found!")
        print("Please ensure the board is connected via USB.")
        return None
    
    if len(ports) == 1 and auto_select:
        print(f"Auto-selected port: {ports[0]}")
        return ports[0]
    
    print("\n=== Available USB Modem Ports ===")
    for idx, port in enumerate(ports, 1):
        print(f"  {idx}. {port}")
    
    while True:
        try:
            selection = input(f"\nSelect port (1-{len(ports)}) or 'q' to quit: ").strip()
            
            if selection.lower() == 'q':
                return None
            
            idx = int(selection)
            if 1 <= idx <= len(ports):
                selected_port = ports[idx - 1]
                print(f"Selected: {selected_port}")
                return selected_port
            else:
                print(f"Invalid selection. Enter a number between 1 and {len(ports)}.")
        except ValueError:
            print("Invalid input. Enter a number or 'q' to quit.")
        except KeyboardInterrupt:
            print("\nCancelled.")
            return None

if __name__ == "__main__":
    # Test the utility
    port = select_port()
    if port:
        print(f"\nYou selected: {port}")
    else:
        print("\nNo port selected.")
        sys.exit(1)
