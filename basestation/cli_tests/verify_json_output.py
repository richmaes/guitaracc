#!/usr/bin/env python3
"""
Verify JSON Output from Device
Tests the actual JSON output against documentation
"""

import serial
import time
import json
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from select_port import select_port

def send_command(ser, cmd, wait_time=1.0, verbose=True):
    """Send command and return response"""
    # Clear any pending data
    ser.reset_input_buffer()
    
    if verbose:
        print(f"  → Sending: {cmd}")
    
    # Send command
    ser.write(f"{cmd}\r\n".encode('utf-8'))
    ser.flush()
    
    # For large outputs like JSON, read incrementally until no more data
    response = ""
    time.sleep(wait_time)
    
    # Keep reading while data is available
    stable_count = 0
    max_reads = 20
    read_count = 0
    
    while stable_count < 3 and read_count < max_reads:  # Wait for 3 consecutive reads with no new data
        read_count += 1
        bytes_waiting = ser.in_waiting
        
        if bytes_waiting > 0:
            chunk = ser.read(bytes_waiting).decode('utf-8', errors='ignore')
            response += chunk
            stable_count = 0
            time.sleep(0.1)  # Small delay between reads
        else:
            stable_count += 1
            time.sleep(0.05)
    
    if verbose:
        print(f"  ← Response length: {len(response)} chars")
        print(f"  ← First 150 chars: {response[:150]}")
    
    return response

def extract_json(text):
    """Extract JSON object from text with ANSI codes"""
    # Remove ANSI escape sequences
    import re
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    clean_text = ansi_escape.sub('', text)
    
    # Find JSON object by brace matching
    brace_count = 0
    start_idx = -1
    
    for i, char in enumerate(clean_text):
        if char == '{':
            if brace_count == 0:
                start_idx = i
            brace_count += 1
        elif char == '}':
            brace_count -= 1
            if brace_count == 0 and start_idx != -1:
                json_str = clean_text[start_idx:i+1]
                return json_str
    
    return None

def test_config_export(ser):
    """Test config export JSON output"""
    print("\n" + "="*60)
    print("Testing: config export")
    print("="*60)
    
    response = send_command(ser, "config export", wait_time=4.0, verbose=True)
    
    # Extract JSON
    json_str = extract_json(response)
    if not json_str:
        print("❌ FAILED: Could not extract JSON from response")
        print("Response snippet:", response[:500])
        return False
    
    try:
        config = json.loads(json_str)
        print("✓ Valid JSON received")
        
        # Verify structure
        checks = [
            ("version field", "version" in config),
            ("config section", "config" in config),
            ("global section", "global" in config.get("config", {})),
            ("patches section", "patches" in config.get("config", {})),
        ]
        
        # Check global fields
        global_cfg = config.get("config", {}).get("global", {})
        global_fields = [
            "default_patch", "midi_channel", "max_guitars", 
            "ble_scan_interval_ms", "led_brightness", "accel_scale",
            "accel_offset", "running_average_enable", "running_average_depth"
        ]
        
        for field in global_fields:
            checks.append((f"global.{field}", field in global_cfg))
        
        # Check patch fields
        patches = config.get("config", {}).get("patches", [])
        if patches:
            patch = patches[0]
            patch_fields = [
                "patch_num", "patch_name", "velocity_curve", "cc_mapping",
                "led_mode", "accel_deadzone", "accel_min", "accel_max", "accel_invert"
            ]
            for field in patch_fields:
                checks.append((f"patch.{field}", field in patch))
        
        # Print results
        all_pass = True
        for name, result in checks:
            status = "✓" if result else "❌"
            print(f"  {status} {name}")
            if not result:
                all_pass = False
        
        # Show sample data
        print("\nSample Global Data:")
        print(f"  MIDI Channel: {global_cfg.get('midi_channel')}")
        print(f"  Accel Scale: {global_cfg.get('accel_scale')}")
        print(f"  Accel Offset: {global_cfg.get('accel_offset')}")
        print(f"  Running Average: {global_cfg.get('running_average_enable')}")
        
        if patches:
            print(f"\nSample Patch Data (Patch 0):")
            print(f"  Name: {patches[0].get('patch_name')}")
            print(f"  CC Mapping: {patches[0].get('cc_mapping')}")
            print(f"  Deadzone: {patches[0].get('accel_deadzone')}")
        
        return all_pass
        
    except json.JSONDecodeError as e:
        print(f"❌ FAILED: JSON decode error: {e}")
        print("JSON string:", json_str[:200])
        return False

def test_pipeline_json(ser):
    """Test pipeline json output"""
    print("\n" + "="*60)
    print("Testing: pipeline json")
    print("="*60)
    
    response = send_command(ser, "pipeline json", wait_time=2.0, verbose=True)
    
    # Extract JSON
    json_str = extract_json(response)
    if not json_str:
        print("❌ FAILED: Could not extract JSON from response")
        print("Response snippet:", response[:500])
        return False
    
    try:
        pipeline = json.loads(json_str)
        print("✓ Valid JSON received")
        
        # Verify structure
        checks = [
            ("patch field", "patch" in pipeline),
            ("rotation section", "rotation" in pipeline),
            ("output section", "output" in pipeline),
            ("conversion section", "conversion" in pipeline),
        ]
        
        rotation = pipeline.get("rotation", {})
        checks.append(("rotation.rho_degrees", "rho_degrees" in rotation))
        checks.append(("rotation.theta_degrees", "theta_degrees" in rotation))
        
        output = pipeline.get("output", {})
        checks.append(("output.midi_cc", "midi_cc" in output))
        
        conversion = pipeline.get("conversion", {})
        checks.append(("conversion.function_type", "function_type" in conversion))
        checks.append(("conversion.parameters", "parameters" in conversion))
        
        # Print results
        all_pass = True
        for name, result in checks:
            status = "✓" if result else "❌"
            print(f"  {status} {name}")
            if not result:
                all_pass = False
        
        # Show sample data
        print("\nPipeline Configuration:")
        print(f"  Patch: {pipeline.get('patch')}")
        print(f"  Rho: {rotation.get('rho_degrees')}°")
        print(f"  Theta: {rotation.get('theta_degrees')}°")
        print(f"  MIDI CC: {output.get('midi_cc')}")
        print(f"  Function: {conversion.get('function_type')}")
        print(f"  Parameters: {conversion.get('parameters')}")
        
        return all_pass
        
    except json.JSONDecodeError as e:
        print(f"❌ FAILED: JSON decode error: {e}")
        print("JSON string:", json_str[:200])
        return False

def test_monitor_json(ser):
    """Test monitor json output"""
    print("\n" + "="*60)
    print("Testing: monitor json")
    print("="*60)
    
    response = send_command(ser, "monitor json", wait_time=2.0, verbose=True)
    
    # Extract JSON (should be single line)
    json_str = extract_json(response)
    if not json_str:
        # Check if it's "No pipeline data available yet"
        if "No pipeline data available" in response or "Waiting for accelerometer" in response:
            print("ℹ️  No pipeline data available yet (no client connected)")
            print("   This is expected if no BLE client is connected")
            return True
        print("❌ FAILED: Could not extract JSON from response")
        print("Response snippet:", response[:500])
        return False
    
    try:
        snapshot = json.loads(json_str)
        print("✓ Valid JSON received")
        
        # Verify structure
        checks = [
            ("timestamp_ms", "timestamp_ms" in snapshot),
            ("raw_axis", "raw_axis" in snapshot),
            ("input_vector", "input_vector" in snapshot),
            ("rotated_vector", "rotated_vector" in snapshot),
            ("normalized_vector", "normalized_vector" in snapshot),
            ("scalar_projection", "scalar_projection" in snapshot),
            ("function_type", "function_type" in snapshot),
            ("midi_output", "midi_output" in snapshot),
        ]
        
        # Check nested structures
        for axis_name in ["raw_axis", "input_vector", "rotated_vector", "normalized_vector"]:
            if axis_name in snapshot:
                axis = snapshot[axis_name]
                checks.append((f"{axis_name}.x", "x" in axis))
                checks.append((f"{axis_name}.y", "y" in axis))
                checks.append((f"{axis_name}.z", "z" in axis))
        
        midi = snapshot.get("midi_output", {})
        checks.append(("midi_output.cc", "cc" in midi))
        checks.append(("midi_output.value", "value" in midi))
        
        # Print results
        all_pass = True
        for name, result in checks:
            status = "✓" if result else "❌"
            print(f"  {status} {name}")
            if not result:
                all_pass = False
        
        # Show sample data
        print("\nPipeline Snapshot:")
        print(f"  Timestamp: {snapshot.get('timestamp_ms')} ms")
        print(f"  Raw Axis: {snapshot.get('raw_axis')}")
        print(f"  Scalar Projection: {snapshot.get('scalar_projection')}")
        print(f"  Function: {snapshot.get('function_type')}")
        print(f"  MIDI Output: CC {midi.get('cc')} = {midi.get('value')}")
        
        return all_pass
        
    except json.JSONDecodeError as e:
        print(f"❌ FAILED: JSON decode error: {e}")
        print("JSON string:", json_str[:200])
        return False

def main():
    """Main test function"""
    print("\n" + "="*60)
    print("GuitarAcc JSON Output Verification")
    print("="*60)
    
    # Select port
    port = select_port(auto_select=True)
    if not port:
        print("No port selected. Exiting.")
        return 1
    
    # Open serial connection
    try:
        ser = serial.Serial(
            port=port,
            baudrate=115200,
            timeout=1,
            rtscts=True
        )
        print(f"\n✓ Connected to {port}")
        time.sleep(0.5)
        
        # Clear any startup messages and get prompt
        time.sleep(1.0)  # Give device time to settle
        
        # Send Enter to get prompt
        print("  Getting prompt...")
        ser.write(b'\r\n')
        ser.flush()
        time.sleep(0.5)
        
        initial_data = ser.read(ser.in_waiting)
        print(f"  Cleared {len(initial_data)} bytes of initial data")
        time.sleep(0.3)
        
    except serial.SerialException as e:
        print(f"❌ Failed to open port {port}: {e}")
        return 1
    
    # Run tests
    results = []
    
    try:
        results.append(("config export", test_config_export(ser)))
        results.append(("pipeline json", test_pipeline_json(ser)))
        results.append(("monitor json", test_monitor_json(ser)))
        
    finally:
        ser.close()
    
    # Summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    
    all_pass = True
    for name, result in results:
        status = "✓ PASS" if result else "❌ FAIL"
        print(f"  {status}: {name}")
        if not result:
            all_pass = False
    
    print("="*60)
    if all_pass:
        print("✓ All JSON outputs match documentation")
        return 0
    else:
        print("❌ Some JSON outputs do not match documentation")
        return 1

if __name__ == "__main__":
    sys.exit(main())
