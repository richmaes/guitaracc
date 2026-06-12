#!/usr/bin/env python3
"""
Test script for accelerometer rotation pipeline monitoring

Connects to basestation serial port and displays pipeline intermediate values
in a formatted, human-readable way for debugging and parameter tuning.

Usage:
    python3 test_pipeline_monitor.py [port]
    
Example:
    python3 test_pipeline_monitor.py /dev/tty.usbmodem0009600164441
"""

import sys
import serial
import time
from datetime import datetime

def format_vector(x, y, z):
    """Format a 3D vector nicely"""
    return f"({x:7.3f}, {y:7.3f}, {z:7.3f})"

def parse_apm_line(line):
    """Parse an APM monitoring line and return structured data"""
    parts = line.split()
    if len(parts) < 15 or parts[0] != 'APM':
        return None
    
    try:
        data = {
            'timestamp_ms': int(parts[1]),
            'accel_x': int(parts[2]),
            'accel_y': int(parts[3]),
            'accel_z': int(parts[4]),
            'rotated_x': float(parts[5]),
            'rotated_y': float(parts[6]),
            'rotated_z': float(parts[7]),
            'normalized_x': float(parts[8]),
            'normalized_y': float(parts[9]),
            'normalized_z': float(parts[10]),
            'scalar': float(parts[11]),
            'midi_value': int(parts[12]),
            'midi_cc': int(parts[13]),
            'func_type': int(parts[14])
        }
        return data
    except (ValueError, IndexError) as e:
        print(f"Error parsing line: {e}")
        return None

def display_pipeline_data(data, show_full=False):
    """Display pipeline data in formatted output"""
    func_names = ['Linear', 'Exponential', 'S-Curve', 'Lookup']
    func_name = func_names[data['func_type']] if data['func_type'] < len(func_names) else 'Unknown'
    
    if show_full:
        print(f"\n{'='*80}")
        print(f"Timestamp: {data['timestamp_ms']} ms")
        print(f"{'='*80}")
        print(f"Stage 1 - Raw Input (milli-g):     {format_vector(data['accel_x'], data['accel_y'], data['accel_z'])}")
        print(f"Stage 2 - After Rotation (g):      {format_vector(data['rotated_x'], data['rotated_y'], data['rotated_z'])}")
        print(f"Stage 3 - Normalized (unit vec):   {format_vector(data['normalized_x'], data['normalized_y'], data['normalized_z'])}")
        print(f"Stage 4 - 1D Projection (scalar):  {data['scalar']:7.3f}")
        print(f"Stage 5 - MIDI Output:              CC{data['midi_cc']:3d} = {data['midi_value']:3d}  [{func_name}]")
    else:
        # Compact single-line output
        print(f"[{data['timestamp_ms']:8d}] "
              f"Accel({data['accel_x']:5d},{data['accel_y']:5d},{data['accel_z']:5d}) → "
              f"Rot{format_vector(data['rotated_x'], data['rotated_y'], data['rotated_z'])} → "
              f"Scalar:{data['scalar']:6.3f} → "
              f"CC{data['midi_cc']:3d}={data['midi_value']:3d}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_pipeline_monitor.py <serial_port>")
        print("\nAvailable ports:")
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        for port in ports:
            print(f"  {port.device} - {port.description}")
        sys.exit(1)
    
    port = sys.argv[1]
    baudrate = 115200
    
    print(f"Connecting to {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2)  # Wait for connection to stabilize
        
        print("\n" + "="*80)
        print("Pipeline Monitor Test - Press Ctrl+C to exit")
        print("="*80)
        print("\nSending command: accel_pipeline_monitor enable")
        
        # Enable monitoring
        ser.write(b"accel_pipeline_monitor enable\n")
        time.sleep(0.5)
        
        # Clear any buffered data
        ser.reset_input_buffer()
        
        print("\nMonitoring pipeline output (compact format):")
        print("-"*80)
        
        line_count = 0
        show_full = False  # Toggle with 'f' key if interactive
        
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if not line:
                continue
            
            # Print non-APM lines (debug output, etc.)
            if not line.startswith('APM'):
                if line.startswith('#') or 'monitoring' in line.lower():
                    print(f"[INFO] {line}")
                continue
            
            # Parse and display APM data
            data = parse_apm_line(line)
            if data:
                display_pipeline_data(data, show_full)
                line_count += 1
                
                # Every 100 lines, show a full detailed output
                if line_count % 100 == 0:
                    print(f"\n[Processed {line_count} samples]")
    
    except serial.SerialException as e:
        print(f"Serial error: {e}")
        sys.exit(1)
    
    except KeyboardInterrupt:
        print("\n\nDisabling monitoring...")
        ser.write(b"accel_pipeline_monitor disable\n")
        time.sleep(0.5)
        print("Monitoring stopped.")
        ser.close()
        print(f"\nTotal samples processed: {line_count}")
    
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
