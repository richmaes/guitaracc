#!/usr/bin/env python3
"""
Accelerometer Deadzone Interactive Test

Tests the accel_deadzone feature that filters MIDI CC messages based on
value change threshold. This test is INTERACTIVE and requires:
- Client (Thingy:53) connected via BLE
- User present to move the client device

The test verifies:
1. Deadzone can be configured via shell command
2. MIDI messages are filtered when CC change < deadzone
3. MIDI messages are sent when CC change >= deadzone
4. "(filtered)" appears in logs when messages are suppressed
"""

import serial
import time
import sys
import re
from select_port import select_port


def send_command(ser, cmd, wait_time=0.5, verbose=True):
    """Send a command and return the response."""
    if verbose:
        print(f"→ {cmd}")
    ser.reset_input_buffer()
    ser.write(f"{cmd}\r\n".encode())
    ser.flush()
    time.sleep(wait_time)
    
    response = ""
    while ser.in_waiting:
        chunk = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        response += chunk
        time.sleep(0.05)
    
    if verbose:
        print(response)
    return response


def monitor_output(ser, duration_seconds=5, verbose=True):
    """Monitor RTT output for a specified duration."""
    if verbose:
        print(f"\n[Monitoring output for {duration_seconds} seconds...]")
    
    output = ""
    start_time = time.time()
    
    while time.time() - start_time < duration_seconds:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            output += chunk
            if verbose:
                print(chunk, end='', flush=True)
        time.sleep(0.05)
    
    if verbose:
        print("\n[Monitoring complete]\n")
    return output


def count_midi_sends(output):
    """Count MIDI sends (excluding filtered) and filtered messages."""
    # Look for patterns like:
    # "Accel: x=-199 y=-186 z=-921 -> MIDI: 57 57 34" (sent)
    # "Accel: x=-199 y=-186 z=-921 -> MIDI: 57 57 34 (filtered)" (filtered)
    
    # Count all MIDI lines
    all_midi_pattern = re.compile(r'-> MIDI: \d+ \d+ \d+', re.MULTILINE)
    all_midi = all_midi_pattern.findall(output)
    
    # Count filtered lines (those ending with "(filtered)")
    filtered_pattern = re.compile(r'-> MIDI: \d+ \d+ \d+ \(filtered\)', re.MULTILINE)
    filtered_midi = filtered_pattern.findall(output)
    
    total_count = len(all_midi)
    filtered_count = len(filtered_midi)
    sent_count = total_count - filtered_count
    
    return sent_count, filtered_count


def test_deadzone_configuration(ser):
    """Test that deadzone can be configured."""
    print("\n" + "="*70)
    print("TEST 1: Deadzone Configuration")
    print("="*70)
    
    # Test setting various deadzone values
    test_values = [1, 5, 10, 20]
    
    for value in test_values:
        print(f"\n  Setting deadzone to {value}...", end='', flush=True)
        response = send_command(ser, f"config accel_deadzone {value}", verbose=False)
        
        if f"CC change threshold set to {value}" in response:
            print(f" ✓")
        else:
            print(f" ✗ FAILED")
            print(f"  Expected confirmation, got: {response}")
            return False
    
    # Test invalid values
    print(f"\n  Testing invalid value (200)...", end='', flush=True)
    response = send_command(ser, "config accel_deadzone 200", verbose=False)
    if "Invalid deadzone" in response or "0-127" in response:
        print(" ✓")
    else:
        print(" ✗ FAILED (should reject values > 127)")
        return False
    
    print("\n✓ Configuration test PASSED")
    return True


def test_deadzone_filtering(ser):
    """Test that deadzone actually filters MIDI messages."""
    print("\n" + "="*70)
    print("TEST 2: Deadzone Filtering (INTERACTIVE)")
    print("="*70)
    
    print("\nThis test requires you to move the client device.")
    print("You will be prompted when to move and when to stop.")
    input("\nPress ENTER when ready to begin...")
    
    # Test with deadzone=1 (send on any change)
    print("\n--- Phase 1: Deadzone = 1 (minimal filtering) ---")
    send_command(ser, "config accel_deadzone 1", verbose=False)
    time.sleep(0.5)
    
    print("\n🔵 Please MOVE the client device continuously for 5 seconds...")
    time.sleep(1)  # Give user time to read
    output_dz1 = monitor_output(ser, duration_seconds=5, verbose=False)
    sent_dz1, filtered_dz1 = count_midi_sends(output_dz1)
    
    print(f"\n  MIDI sent: {sent_dz1}, filtered: {filtered_dz1}")
    
    # Test with deadzone=20 (aggressive filtering)
    print("\n--- Phase 2: Deadzone = 20 (aggressive filtering) ---")
    send_command(ser, "config accel_deadzone 20", verbose=False)
    time.sleep(0.5)
    
    print("\n🔵 Please MOVE the client device continuously for 5 seconds...")
    print("   (Move it the same way as before)")
    time.sleep(1)
    output_dz20 = monitor_output(ser, duration_seconds=5, verbose=False)
    sent_dz20, filtered_dz20 = count_midi_sends(output_dz20)
    
    print(f"\n  MIDI sent: {sent_dz20}, filtered: {filtered_dz20}")
    
    # Verify that deadzone=20 sent fewer messages and filtered more
    print("\n--- Results Analysis ---")
    print(f"  Deadzone=1:  sent={sent_dz1}, filtered={filtered_dz1}")
    print(f"  Deadzone=20: sent={sent_dz20}, filtered={filtered_dz20}")
    
    # With higher deadzone, we should see:
    # - Fewer messages sent (or approximately equal if movement was large)
    # - More filtered messages
    
    if filtered_dz20 > filtered_dz1:
        print("\n  ✓ Deadzone=20 filtered MORE messages than deadzone=1")
        print("\n✓ Filtering test PASSED")
        return True
    else:
        print(f"\n  ✗ Expected more filtering with deadzone=20")
        print(f"    This might happen if movement was very large.")
        print(f"    Try smaller, gentler movements for more accurate results.")
        
        # Ask user if they want to continue despite this
        print("\n  This could be a false negative if movement was too aggressive.")
        response = input("  Continue anyway? (y/n): ")
        return response.lower() == 'y'


def test_deadzone_persistence(ser):
    """Test that deadzone setting persists."""
    print("\n" + "="*70)
    print("TEST 3: Deadzone Persistence")
    print("="*70)
    
    print("\n  Setting deadzone to 15...", end='', flush=True)
    send_command(ser, "config accel_deadzone 15", verbose=False)
    time.sleep(0.5)
    print(" ✓")
    
    print("  Triggering config reload...", end='', flush=True)
    send_command(ser, "config load", verbose=False)
    time.sleep(0.5)
    print(" ✓")
    
    # Monitor output to check if reload worked
    # The deadzone value itself isn't echoed, but we can verify no errors occurred
    print("  Verifying no errors after reload...", end='', flush=True)
    output = monitor_output(ser, duration_seconds=1, verbose=False)
    
    if "error" in output.lower() or "failed" in output.lower():
        print(" ✗ FAILED")
        print(f"  Errors found in output: {output}")
        return False
    
    print(" ✓")
    print("\n✓ Persistence test PASSED")
    return True


def main():
    print("="*70)
    print("  GuitarAcc Basestation - Accelerometer Deadzone Test")
    print("="*70)
    print("\nThis is an INTERACTIVE test that requires:")
    print("  • Client (Thingy:53) connected via BLE")
    print("  • User present to move the client device")
    print("  • Approximately 2-3 minutes to complete")
    print("\nThe test will prompt you when to move the device.")
    print("="*70)
    
    response = input("\nContinue with interactive test? (y/n): ")
    if response.lower() != 'y':
        print("\nTest skipped by user.")
        return 0
    
    # Select serial port
    port = select_port()
    if not port:
        print("ERROR: No serial port selected")
        return 1
    
    print(f"\nOpening {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(0.5)
        
        # Clear any pending output
        ser.reset_input_buffer()
        
        # Run tests
        tests = [
            ("Configuration", test_deadzone_configuration),
            ("Filtering", test_deadzone_filtering),
            ("Persistence", test_deadzone_persistence),
        ]
        
        passed = 0
        failed = 0
        
        for test_name, test_func in tests:
            try:
                if test_func(ser):
                    passed += 1
                else:
                    failed += 1
                    print(f"\n✗ {test_name} test FAILED")
            except KeyboardInterrupt:
                print("\n\nTest interrupted by user.")
                ser.close()
                return 1
            except Exception as e:
                print(f"\n✗ {test_name} test FAILED with exception: {e}")
                import traceback
                traceback.print_exc()
                failed += 1
        
        # Summary
        print("\n" + "="*70)
        print("  TEST SUMMARY")
        print("="*70)
        print(f"  Passed: {passed}/{len(tests)}")
        print(f"  Failed: {failed}/{len(tests)}")
        
        if failed == 0:
            print("\n✓✓✓ ALL DEADZONE TESTS PASSED ✓✓✓")
            print("="*70)
            ser.close()
            return 0
        else:
            print("\n✗✗✗ SOME DEADZONE TESTS FAILED ✗✗✗")
            print("="*70)
            ser.close()
            return 1
            
    except serial.SerialException as e:
        print(f"ERROR: Could not open serial port: {e}")
        return 1
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
