#!/usr/bin/env python3
"""
Integration Test Script for Virtual Ports Feature
Tests the fully integrated virtual ports system on the basestation hardware.

This script assumes the basestation is running with virtual ports integrated.
It connects via UART and exercises the new CLI commands to verify integration.

Usage:
  python test_vports_integration.py [--port /dev/ttyACM0] [--baud 115200]
"""

import serial
import time
import argparse
import sys
import re

class BasestationTester:
    def __init__(self, port, baud=115200, timeout=2.0):
        """Initialize connection to basestation"""
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        
    def connect(self):
        """Connect to basestation via UART"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
            time.sleep(0.5)  # Wait for connection to stabilize
            # Clear any buffered data
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print(f"✓ Connected to {self.port} at {self.baud} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from basestation"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✓ Disconnected")
    
    def send_command(self, cmd, wait_time=0.5):
        """Send command and return response"""
        if not self.ser or not self.ser.is_open:
            return None
        
        # Send command
        self.ser.write(f"{cmd}\r\n".encode())
        time.sleep(wait_time)
        
        # Read response
        response = ""
        while self.ser.in_waiting > 0:
            response += self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        return response
    
    def verify_output(self, response, expected_patterns):
        """Verify response contains expected patterns"""
        if not response:
            return False
        
        for pattern in expected_patterns:
            if not re.search(pattern, response, re.IGNORECASE):
                print(f"  ✗ Expected pattern not found: {pattern}")
                return False
        return True

def test_basic_connectivity(tester):
    """Test 1: Basic connectivity and command execution"""
    print("\n[Test 1] Basic Connectivity")
    print("-" * 50)
    
    response = tester.send_command("status")
    if response and "basestation" in response.lower():
        print("  ✓ Device responds to commands")
        return True
    else:
        print("  ✗ No response from device")
        return False

def test_config_show(tester):
    """Test 2: Configuration loading"""
    print("\n[Test 2] Configuration Loading")
    print("-" * 50)
    
    response = tester.send_command("config show")
    if tester.verify_output(response, ["MIDI", "Patch", "CC"]):
        print("  ✓ Configuration loaded successfully")
        return True
    else:
        print("  ✗ Failed to load configuration")
        return False

def test_topology_show(tester):
    """Test 3: Topology configuration visibility"""
    print("\n[Test 3] Topology Configuration")
    print("-" * 50)
    
    response = tester.send_command("topo show")
    if response:
        print("  ✓ Topology show command executed")
        if "Virtual Ports" in response or "Topology" in response:
            print("  ✓ Topology configuration displayed")
            return True
        else:
            print("  ⚠ Topology show executed but format unexpected")
            return True  # Not a failure, just unexpected format
    else:
        print("  ✗ Topology show command failed")
        return False

def test_function_show(tester):
    """Test 4: Function unit visibility"""
    print("\n[Test 4] Function Unit Configuration")
    print("-" * 50)
    
    response = tester.send_command("func show 0")
    if response:
        print("  ✓ Function show command executed")
        if "Function Unit" in response or "Type:" in response:
            print("  ✓ Function unit 0 displayed")
            return True
        else:
            print("  ⚠ Function show executed but format unexpected")
            return True
    else:
        print("  ✗ Function show command failed")
        return False

def test_topology_enable(tester):
    """Test 5: Enable/disable virtual ports"""
    print("\n[Test 5] Enable/Disable Virtual Ports")
    print("-" * 50)
    
    # Enable topology
    response = tester.send_command("topo enable 1", wait_time=1.0)
    if response and "ENABLED" in response:
        print("  ✓ Virtual ports enabled")
    else:
        print("  ⚠ Enable command may have failed")
    
    # Verify with show
    response = tester.send_command("topo show")
    if response and "ENABLED" in response:
        print("  ✓ Virtual ports status confirmed as ENABLED")
        success = True
    else:
        print("  ✗ Virtual ports not showing as enabled")
        success = False
    
    # Disable topology (restore legacy mode)
    response = tester.send_command("topo enable 0", wait_time=1.0)
    if response and "DISABLED" in response:
        print("  ✓ Virtual ports disabled (legacy mode)")
    
    return success

def test_topology_config(tester):
    """Test 6: Configure topology instance"""
    print("\n[Test 6] Configure Topology Instance")
    print("-" * 50)
    
    # Configure instance 0 as T1 (simple linear)
    response = tester.send_command("topo config 0 1 0 0 16", wait_time=1.0)
    if response and ("configured" in response.lower() or "T1" in response):
        print("  ✓ Topology instance 0 configured as T1")
    else:
        print("  ⚠ Topology config may have failed")
    
    # Verify configuration
    response = tester.send_command("topo show")
    if response and "T1" in response:
        print("  ✓ Topology configuration verified")
        return True
    else:
        print("  ✗ Topology configuration not visible")
        return False

def test_function_config(tester):
    """Test 7: Configure function unit"""
    print("\n[Test 7] Configure Function Unit")
    print("-" * 50)
    
    # Configure function 0 as LINEAR with ±2g range
    response = tester.send_command("func linear 0 -2000 2000 0 127", wait_time=1.0)
    if response and ("configured" in response.lower() or "LINEAR" in response):
        print("  ✓ Function unit 0 configured as LINEAR")
    else:
        print("  ⚠ Function config may have failed")
    
    # Verify configuration
    response = tester.send_command("func show 0")
    if response and "LINEAR" in response:
        print("  ✓ Function configuration verified")
        return True
    else:
        print("  ✗ Function configuration not visible")
        return False

def test_config_reload(tester):
    """Test 8: Configuration reload"""
    print("\n[Test 8] Configuration Reload")
    print("-" * 50)
    
    response = tester.send_command("config reload", wait_time=1.5)
    if response and ("reload" in response.lower() or "loaded" in response.lower()):
        print("  ✓ Configuration reload executed")
        return True
    else:
        print("  ✗ Configuration reload failed")
        return False

def test_config_save(tester):
    """Test 9: Configuration persistence"""
    print("\n[Test 9] Configuration Persistence")
    print("-" * 50)
    
    response = tester.send_command("config save", wait_time=2.0)
    if response and ("saved" in response.lower() or "success" in response.lower()):
        print("  ✓ Configuration saved to flash")
        return True
    else:
        print("  ⚠ Configuration save may have failed")
        return False

def test_build_verification(tester):
    """Test 10: Verify build includes virtual ports modules"""
    print("\n[Test 10] Build Verification")
    print("-" * 50)
    
    # Check if new commands are available
    response = tester.send_command("help", wait_time=1.0)
    if response:
        if "topo" in response.lower() and "func" in response.lower():
            print("  ✓ New commands (topo, func) are available")
            return True
        else:
            print("  ✗ New commands not found in help")
            return False
    else:
        print("  ✗ Help command failed")
        return False

def run_all_tests(tester):
    """Run complete integration test suite"""
    print("\n" + "=" * 50)
    print("Virtual Ports Integration Test Suite")
    print("=" * 50)
    
    tests = [
        ("Basic Connectivity", test_basic_connectivity),
        ("Configuration Loading", test_config_show),
        ("Topology Visibility", test_topology_show),
        ("Function Visibility", test_function_show),
        ("Topology Enable/Disable", test_topology_enable),
        ("Topology Configuration", test_topology_config),
        ("Function Configuration", test_function_config),
        ("Configuration Reload", test_config_reload),
        ("Configuration Persistence", test_config_save),
        ("Build Verification", test_build_verification),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            result = test_func(tester)
            results.append((name, result))
        except Exception as e:
            print(f"  ✗ Test raised exception: {e}")
            results.append((name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("Test Summary")
    print("=" * 50)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {status}: {name}")
    
    print(f"\nTotal: {passed}/{total} tests passed ({100*passed//total}%)")
    
    if passed == total:
        print("\n✓ All integration tests PASSED!")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) FAILED")
        return 1

def main():
    parser = argparse.ArgumentParser(description="Virtual Ports Integration Test")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port (default: /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=2.0, help="Command timeout (default: 2.0)")
    
    args = parser.parse_args()
    
    tester = BasestationTester(args.port, args.baud, args.timeout)
    
    if not tester.connect():
        print("\nFailed to connect to basestation!")
        print("Make sure:")
        print("  1. Device is connected to", args.port)
        print("  2. No other program is using the port")
        print("  3. Correct baud rate is set")
        return 1
    
    try:
        exit_code = run_all_tests(tester)
    finally:
        tester.disconnect()
    
    return exit_code

if __name__ == "__main__":
    sys.exit(main())
