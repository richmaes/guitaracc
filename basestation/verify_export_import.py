#!/usr/bin/env python3
"""
Configuration Export/Import Verification Script

Tests the config export and import functionality:
- Export full configuration and validate JSON
- Export global-only and validate
- Export single patch and validate
- Verify exported data matches actual device config
"""

import serial
import time
import sys
import json
import tempfile
import os
from select_port import select_port


def send_command(ser, cmd, wait_time=0.5, verbose=False):
    """Send a command and return the response."""
    if verbose:
        print(f"→ {cmd}")
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
    
    if verbose:
        print(response)
    return response


def extract_json_from_response(response):
    """Extract JSON object from shell response."""
    import re
    
    # Remove ANSI escape codes
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    response = ansi_escape.sub('', response)
    
    # Find JSON object boundaries
    start_idx = response.find('{')
    if start_idx == -1:
        print("No JSON object found in response")
        print(f"Raw response:\n{response}")
        return None
    
    # Count braces to find the matching closing brace
    brace_count = 0
    end_idx = start_idx
    
    for i in range(start_idx, len(response)):
        if response[i] == '{':
            brace_count += 1
        elif response[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                end_idx = i + 1
                break
    
    if brace_count != 0:
        print("Unmatched braces in JSON")
        print(f"Raw response:\n{response}")
        return None
    
    json_text = response[start_idx:end_idx]
    
    try:
        return json.loads(json_text)
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}")
        print(f"Extracted JSON text:\n{json_text}")
        print(f"\nRaw response:\n{response}")
        return None


def test_export_full(ser):
    """Test full configuration export."""
    print("  Testing full export...", end='', flush=True)
    
    # Export full config needs longer timeout for 16 patches
    response = send_command(ser, "config export", wait_time=3.0)
    config = extract_json_from_response(response)
    
    if config is None:
        print(" ✗ FAILED (invalid JSON)")
        return False
    
    # Validate structure
    if 'version' not in config:
        print(" ✗ FAILED (missing version)")
        return False
    
    if 'config' not in config:
        print(" ✗ FAILED (missing config section)")
        return False
    
    config_section = config['config']
    
    if 'global' not in config_section:
        print(" ✗ FAILED (missing global section)")
        return False
    
    if 'patches' not in config_section:
        print(" ✗ FAILED (missing patches section)")
        return False
    
    patches = config_section['patches']
    if len(patches) != 16:
        print(f" ✗ FAILED (expected 16 patches, got {len(patches)})")
        return False
    
    # Validate global fields
    global_cfg = config_section['global']
    required_global = ['default_patch', 'midi_channel', 'max_guitars', 
                       'ble_scan_interval_ms', 'led_brightness', 'accel_scale',
                       'running_average_enable', 'running_average_depth']
    
    for field in required_global:
        if field not in global_cfg:
            print(f" ✗ FAILED (missing global.{field})")
            return False
    
    # Validate patch fields
    required_patch = ['patch_num', 'patch_name', 'velocity_curve', 'cc_mapping',
                      'led_mode', 'accel_deadzone', 'accel_min', 'accel_max', 'accel_invert']
    
    for i, patch in enumerate(patches):
        for field in required_patch:
            if field not in patch:
                print(f" ✗ FAILED (patch {i} missing {field})")
                return False
        
        # Validate patch number
        if patch['patch_num'] != i:
            print(f" ✗ FAILED (patch {i} has wrong patch_num: {patch['patch_num']})")
            return False
    
    print(" ✓")
    return config


def test_export_global(ser):
    """Test global-only export."""
    print("  Testing global-only export...", end='', flush=True)
    
    response = send_command(ser, "config export global", wait_time=1.0)
    config = extract_json_from_response(response)
    
    if config is None:
        print(" ✗ FAILED (invalid JSON)")
        return False
    
    config_section = config.get('config', {})
    
    if 'global' not in config_section:
        print(" ✗ FAILED (missing global section)")
        return False
    
    if 'patches' in config_section:
        print(" ✗ FAILED (patches should not be present)")
        return False
    
    print(" ✓")
    return config


def test_export_single_patch(ser, patch_num=5):
    """Test single patch export."""
    print(f"  Testing single patch export (patch {patch_num})...", end='', flush=True)
    
    response = send_command(ser, f"config export patch {patch_num}", wait_time=1.0)
    config = extract_json_from_response(response)
    
    if config is None:
        print(" ✗ FAILED (invalid JSON)")
        return False
    
    config_section = config.get('config', {})
    
    if 'global' in config_section:
        print(" ✗ FAILED (global should not be present)")
        return False
    
    if 'patches' not in config_section:
        print(" ✗ FAILED (missing patches section)")
        return False
    
    patches = config_section['patches']
    if len(patches) != 1:
        print(f" ✗ FAILED (expected 1 patch, got {len(patches)})")
        return False
    
    if patches[0]['patch_num'] != patch_num:
        print(f" ✗ FAILED (wrong patch number: {patches[0]['patch_num']})")
        return False
    
    print(" ✓")
    return config


def test_export_consistency(ser, full_config):
    """Test that multiple exports return consistent data."""
    print("  Testing export consistency...", end='', flush=True)
    
    # Export again
    response = send_command(ser, "config export", wait_time=2.0)
    config2 = extract_json_from_response(response)
    
    if config2 is None:
        print(" ✗ FAILED (second export invalid)")
        return False
    
    # Compare global sections
    if full_config['config']['global'] != config2['config']['global']:
        print(" ✗ FAILED (global sections differ)")
        return False
    
    # Compare patches
    for i in range(16):
        patch1 = full_config['config']['patches'][i]
        patch2 = config2['config']['patches'][i]
        
        if patch1 != patch2:
            print(f" ✗ FAILED (patch {i} differs)")
            return False
    
    print(" ✓")
    return True


def test_validate_ranges(full_config):
    """Validate that exported values are within expected ranges."""
    print("  Validating field ranges...", end='', flush=True)
    
    global_cfg = full_config['config']['global']
    
    # Check MIDI channel
    if not (0 <= global_cfg['midi_channel'] <= 15):
        print(f" ✗ FAILED (midi_channel {global_cfg['midi_channel']} out of range)")
        return False
    
    # Check running average depth
    if not (3 <= global_cfg['running_average_depth'] <= 10):
        print(f" ✗ FAILED (running_average_depth {global_cfg['running_average_depth']} out of range)")
        return False
    
    # Check patches
    for i, patch in enumerate(full_config['config']['patches']):
        # Check velocity curve
        if not (0 <= patch['velocity_curve'] <= 127):
            print(f" ✗ FAILED (patch {i} velocity_curve out of range)")
            return False
        
        # Check CC mapping
        for j, cc in enumerate(patch['cc_mapping']):
            if not (0 <= cc <= 127):
                print(f" ✗ FAILED (patch {i} cc_mapping[{j}] = {cc} out of range)")
                return False
        
        # Check accel_min/max
        for field in ['accel_min', 'accel_max']:
            for j, val in enumerate(patch[field]):
                if not (0 <= val <= 127):
                    print(f" ✗ FAILED (patch {i} {field}[{j}] = {val} out of range)")
                    return False
        
        # Check accel_invert is a valid byte
        if not (0 <= patch['accel_invert'] <= 255):
            print(f" ✗ FAILED (patch {i} accel_invert out of range)")
            return False
    
    print(" ✓")
    return True


def test_save_and_reload(full_config):
    """Test saving exported config to file and reloading."""
    print("  Testing save/reload from file...", end='', flush=True)
    
    try:
        # Create temp file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            temp_file = f.name
            json.dump(full_config, f, indent=2)
        
        # Reload and compare
        with open(temp_file, 'r') as f:
            reloaded = json.load(f)
        
        if reloaded != full_config:
            print(" ✗ FAILED (reloaded config differs)")
            os.unlink(temp_file)
            return False
        
        os.unlink(temp_file)
        print(" ✓")
        return True
        
    except Exception as e:
        print(f" ✗ FAILED ({e})")
        return False


def test_apply_config_via_cli(ser):
    """
    Test applying configuration via CLI commands and verifying the results.
    This simulates manual configuration import workflow.
    """
    print("  Resetting to factory defaults...", end='', flush=True)
    
    # Reset to factory defaults
    response = send_command(ser, "config restore", wait_time=1.0)
    if "Factory defaults restored" not in response:
        print(f" ✗ FAILED (restore failed: {response})")
        return False
    print(" ✓")
    
    print("  Applying test configuration...", end='', flush=True)
    
    # Define a test configuration to apply
    test_config = {
        'midi_channel': 5,
        'patch_num': 3,
        'cc_x': 10,
        'cc_y': 11,
        'cc_z': 12,
        'velocity_curve': 42,
        'accel_min': [20, 21, 22, 23, 24, 25],
        'accel_max': [100, 101, 102, 103, 104, 105],
        'accel_invert': [True, False, True, False, True, False],
        'scan_interval': 250,
        'avg_enable': False,
        'avg_depth': 7
    }
    
    # Apply configuration via CLI commands
    commands = [
        f"config midi_ch {test_config['midi_channel'] + 1}",  # MIDI channel is 1-16
        f"config select {test_config['patch_num']}",
        f"config cc x {test_config['cc_x']}",
        f"config cc y {test_config['cc_y']}",
        f"config cc z {test_config['cc_z']}",
        f"config velocity_curve {test_config['velocity_curve']}",
    ]
    
    # Add accel_min settings
    for i in range(6):
        commands.append(f"config accel_min {i} {test_config['accel_min'][i]}")
    
    # Add accel_max settings
    for i in range(6):
        commands.append(f"config accel_max {i} {test_config['accel_max'][i]}")
    
    # Add accel_invert settings
    for i in range(6):
        commands.append(f"config accel_invert {i} {1 if test_config['accel_invert'][i] else 0}")
    
    # Add global settings
    commands.extend([
        f"config scan_interval {test_config['scan_interval']}",
        f"config avg_enable {1 if test_config['avg_enable'] else 0}",
        f"config avg_depth {test_config['avg_depth']}",
        "config save"
    ])
    
    # Execute all commands
    for cmd in commands:
        response = send_command(ser, cmd, wait_time=0.3)
        if "Error" in response or "error" in response:
            print(f"\n  ✗ FAILED (command '{cmd}' returned error)")
            return False
    
    print(" ✓")
    
    print("  Verifying applied configuration...", end='', flush=True)
    
    # Export and verify the configuration was applied
    response = send_command(ser, "config export", wait_time=3.0)
    config = extract_json_from_response(response)
    
    if config is None:
        print(" ✗ FAILED (export failed)")
        return False
    
    # Verify global settings
    global_cfg = config['config']['global']
    if global_cfg['midi_channel'] != test_config['midi_channel']:
        print(f"\n  ✗ FAILED (MIDI channel: expected {test_config['midi_channel']}, got {global_cfg['midi_channel']})")
        return False
    
    if global_cfg['ble_scan_interval_ms'] != test_config['scan_interval']:
        print(f"\n  ✗ FAILED (scan interval: expected {test_config['scan_interval']}, got {global_cfg['ble_scan_interval_ms']})")
        return False
    
    if global_cfg['running_average_enable'] != test_config['avg_enable']:
        print(f"\n  ✗ FAILED (avg enable: expected {test_config['avg_enable']}, got {global_cfg['running_average_enable']})")
        return False
    
    if global_cfg['running_average_depth'] != test_config['avg_depth']:
        print(f"\n  ✗ FAILED (avg depth: expected {test_config['avg_depth']}, got {global_cfg['running_average_depth']})")
        return False
    
    # Verify patch settings
    patch = config['config']['patches'][test_config['patch_num']]
    
    if patch['cc_mapping'][0] != test_config['cc_x']:
        print(f"\n  ✗ FAILED (CC X: expected {test_config['cc_x']}, got {patch['cc_mapping'][0]})")
        return False
    
    if patch['cc_mapping'][1] != test_config['cc_y']:
        print(f"\n  ✗ FAILED (CC Y: expected {test_config['cc_y']}, got {patch['cc_mapping'][1]})")
        return False
    
    if patch['cc_mapping'][2] != test_config['cc_z']:
        print(f"\n  ✗ FAILED (CC Z: expected {test_config['cc_z']}, got {patch['cc_mapping'][2]})")
        return False
    
    if patch['velocity_curve'] != test_config['velocity_curve']:
        print(f"\n  ✗ FAILED (velocity curve: expected {test_config['velocity_curve']}, got {patch['velocity_curve']})")
        return False
    
    # Verify accel_min
    for i in range(6):
        if patch['accel_min'][i] != test_config['accel_min'][i]:
            print(f"\n  ✗ FAILED (accel_min[{i}]: expected {test_config['accel_min'][i]}, got {patch['accel_min'][i]})")
            return False
    
    # Verify accel_max
    for i in range(6):
        if patch['accel_max'][i] != test_config['accel_max'][i]:
            print(f"\n  ✗ FAILED (accel_max[{i}]: expected {test_config['accel_max'][i]}, got {patch['accel_max'][i]})")
            return False
    
    # Verify accel_invert
    expected_invert = 0
    for i in range(6):
        if test_config['accel_invert'][i]:
            expected_invert |= (1 << i)
    
    if patch['accel_invert'] != expected_invert:
        print(f"\n  ✗ FAILED (accel_invert: expected {expected_invert}, got {patch['accel_invert']})")
        return False
    
    print(" ✓")
    print("  All settings verified correctly!")
    
    return True


def main():
    port = select_port(auto_select=True)
    if port is None:
        print("No port selected. Exiting.")
        sys.exit(1)
    
    try:
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        print(f"Connected to {port}")
        print("=" * 70)
        print("CONFIGURATION EXPORT/IMPORT VERIFICATION TEST")
        print("=" * 70)
        time.sleep(1)
        
        all_passed = True
        
        # Test 1: Full export
        print("\n[TEST 1] Full Configuration Export")
        full_config = test_export_full(ser)
        if not full_config:
            all_passed = False
        
        # Test 2: Global-only export
        print("\n[TEST 2] Global-Only Export")
        if not test_export_global(ser):
            all_passed = False
        
        # Test 3: Single patch export
        print("\n[TEST 3] Single Patch Export")
        if not test_export_single_patch(ser, 5):
            all_passed = False
        
        # Test 4: Export consistency
        if full_config:
            print("\n[TEST 4] Export Consistency")
            if not test_export_consistency(ser, full_config):
                all_passed = False
        
        # Test 5: Validate ranges
        if full_config:
            print("\n[TEST 5] Field Range Validation")
            if not test_validate_ranges(full_config):
                all_passed = False
        
        # Test 6: Save and reload
        if full_config:
            print("\n[TEST 6] Save/Reload from File")
            if not test_save_and_reload(full_config):
                all_passed = False
        
        # Test 7: Apply configuration via CLI and verify
        print("\n[TEST 7] Apply Config via CLI and Verify")
        if not test_apply_config_via_cli(ser):
            all_passed = False
        
        # Summary
        print("\n" + "=" * 70)
        print("VERIFICATION SUMMARY")
        print("=" * 70)
        
        if all_passed:
            print("✓ All export/import tests passed")
            print("✓ JSON format validation: PASS")
            print("✓ Field range validation: PASS")
            print("✓ Export consistency: PASS")
        else:
            print("✗ Some tests failed")
        
        print("=" * 70)
        
        ser.close()
        
        if all_passed:
            print("\n✓✓✓ ALL TESTS PASSED ✓✓✓")
            sys.exit(0)
        else:
            print("\n✗✗✗ SOME TESTS FAILED ✗✗✗")
            sys.exit(1)
        
    except Exception as e:
        print(f'Error: {e}')
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
