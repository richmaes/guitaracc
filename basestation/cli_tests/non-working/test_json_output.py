#!/usr/bin/env python3
"""
Test JSON Output for Pipeline Commands

Tests the pipeline json and pipeline_monitor json commands to verify
they produce valid JSON output.
"""

import serial
import time
import sys
import json
import re
import argparse
from select_port import select_port


def send_command(ser, cmd, wait_time=0.5):
    """Send a command and return the response."""
    ser.reset_input_buffer()
    ser.write(f"{cmd}\r\n".encode())
    ser.flush()
    
    # For large outputs like JSON, read incrementally until no more data
    response = ""
    time.sleep(wait_time)
    
    # Keep reading while data is available
    stable_count = 0
    while stable_count < 3:  # Wait for 3 consecutive reads with no new data
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            response += chunk
            stable_count = 0
            time.sleep(0.1)  # Small delay between reads
        else:
            stable_count += 1
            time.sleep(0.05)
    
    return response


def strip_ansi(text):
    """Remove ANSI escape codes from text."""
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)


def extract_json(response):
    """Extract JSON object from response text."""
    clean_text = strip_ansi(response)
    
    # Find JSON object boundaries
    start_idx = clean_text.find('{')
    if start_idx == -1:
        return None
    
    # Count braces to find the matching closing brace
    brace_count = 0
    end_idx = start_idx
    
    for i in range(start_idx, len(clean_text)):
        if clean_text[i] == '{':
            brace_count += 1
        elif clean_text[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                end_idx = i + 1
                break
    
    json_text = clean_text[start_idx:end_idx]
    return json_text


def test_pipeline_json(port):
    """Test the 'pipeline json' command."""
    print("\n" + "="*60)
    print("Testing: pipeline json")
    print("="*60)
    
    try:
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        time.sleep(1)
        
        response = send_command(ser, "pipeline json", wait_time=1.0)
        ser.close()
        
        print("\nRaw response:")
        print("-" * 60)
        print(response)
        print("-" * 60)
        
        # Try to extract and parse JSON
        json_text = extract_json(response)
        if json_text:
            print("\nExtracted JSON:")
            print("-" * 60)
            print(json_text)
            print("-" * 60)
            
            try:
                data = json.loads(json_text)
                print("\n✓ Valid JSON!")
                print("\nParsed data:")
                print(json.dumps(data, indent=2))
                return True
            except json.JSONDecodeError as e:
                print(f"\n✗ JSON Parse Error: {e}")
                return False
        else:
            print("\n✗ No JSON object found in response")
            return False
            
    except Exception as e:
        print(f"\n✗ Error: {e}")
        return False


def test_pipeline_monitor_json(port):
    """Test the 'pipeline_monitor json' command."""
    print("\n" + "="*60)
    print("Testing: pipeline_monitor json")
    print("="*60)
    
    try:
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        time.sleep(1)
        
        # Enable JSON monitoring
        print("\nEnabling JSON monitoring...")
        response = send_command(ser, "pipeline_monitor json", wait_time=1.0)
        print("Response:", response[:200])
        
        # Wait for some monitoring output
        print("\nWaiting for monitoring data (5 seconds)...")
        time.sleep(5)
        
        # Read monitoring output
        monitoring_data = ""
        if ser.in_waiting:
            monitoring_data = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        
        # Disable monitoring
        send_command(ser, "pipeline_monitor disable", wait_time=0.5)
        ser.close()
        
        if not monitoring_data:
            print("\n✗ No monitoring data received")
            print("   (This is normal if no accelerometer data is being received)")
            return False
        
        print("\nMonitoring output received:")
        print("-" * 60)
        # Show first few lines
        lines = monitoring_data.split('\n')[:10]
        for line in lines:
            print(line)
        print("-" * 60)
        
        # Try to parse JSON lines
        clean_data = strip_ansi(monitoring_data)
        json_lines = [line for line in clean_data.split('\n') if line.strip().startswith('{')]
        
        if not json_lines:
            print("\n✗ No JSON lines found in monitoring output")
            return False
        
        print(f"\nFound {len(json_lines)} JSON line(s)")
        
        # Try to parse first JSON line
        try:
            data = json.loads(json_lines[0])
            print("\n✓ Valid JSON monitoring output!")
            print("\nFirst sample:")
            print(json.dumps(data, indent=2))
            
            # Verify expected fields
            expected_fields = [
                'timestamp_ms', 'raw_axis', 'input_vector', 'rotated_vector',
                'normalized_vector', 'scalar_projection', 'function_type', 'midi_output'
            ]
            missing = [f for f in expected_fields if f not in data]
            if missing:
                print(f"\n⚠ Warning: Missing fields: {missing}")
            else:
                print("\n✓ All expected fields present")
            
            return True
        except json.JSONDecodeError as e:
            print(f"\n✗ JSON Parse Error: {e}")
            print(f"Problematic line: {json_lines[0]}")
            return False
            
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    parser = argparse.ArgumentParser(description='Test JSON output for pipeline commands')
    parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
    args = parser.parse_args()
    
    print("JSON Output Test for Pipeline Commands")
    print("=" * 60)
    
    # Select port
    port = args.port if args.port else select_port(auto_select=True)
    if not port:
        print("No port selected. Exiting.")
        sys.exit(1)
    
    # Test both commands
    test1 = test_pipeline_json(port)
    test2 = test_pipeline_monitor_json(port)
    
    # Summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)
    print(f"pipeline json:          {'PASS ✓' if test1 else 'FAIL ✗'}")
    print(f"pipeline_monitor json:  {'PASS ✓' if test2 else 'FAIL ✗'}")
    print("="*60)
    
    if test1 and test2:
        print("\n✓ All tests passed!")
        sys.exit(0)
    else:
        print("\n✗ Some tests failed. Check output above.")
        sys.exit(1)


if __name__ == "__main__":
    main()
