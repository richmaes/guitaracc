#!/usr/bin/env python3
"""
Accelerometer Pipeline Validation Test

Tests the complete accelerometer processing pipeline by:
1. Configuring pipeline with known rho/theta/function parameters
2. Prompting user to orient device to specific positions
3. Reading monitor json output
4. Calculating expected values through each pipeline stage
5. Comparing firmware output with expected values within tolerance

This test validates:
- Rotation matrix calculations (rho around X, theta around Y)
- Vector normalization
- Scalar projection (X-component extraction)
- Conversion functions (linear, exponential, s-curve)
- MIDI value generation (0-127)
"""
import serial
import time
import sys
import json
import math
import argparse
from select_port import select_port

TEST_NAME = "Accelerometer Pipeline Validation"

# Tolerance for floating-point comparisons
TOLERANCE_VECTOR = 0.02  # ±0.02 for vector components (g units)
TOLERANCE_SCALAR = 0.02  # ±0.02 for scalar projection
TOLERANCE_MIDI = 3       # ±3 for MIDI values (0-127)

def send_command(ser, cmd, wait_time=0.5, verbose=True):
    """Send a command and return the response."""
    if verbose:
        print(f"→ {cmd}")
    ser.reset_input_buffer()
    ser.write(f"{cmd}\r\n".encode())
    ser.flush()
    time.sleep(wait_time)
    
    response = ""
    if ser.in_waiting:
        response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        if verbose:
            print(response)
    return response


def rotate_vector_x(v, rho_deg):
    """Rotate vector around X axis by rho degrees."""
    rho_rad = math.radians(rho_deg)
    cos_rho = math.cos(rho_rad)
    sin_rho = math.sin(rho_rad)
    
    x = v[0]
    y = v[1] * cos_rho - v[2] * sin_rho
    z = v[1] * sin_rho + v[2] * cos_rho
    
    return (x, y, z)


def rotate_vector_y(v, theta_deg):
    """Rotate vector around Y axis by theta degrees."""
    theta_rad = math.radians(theta_deg)
    cos_theta = math.cos(theta_rad)
    sin_theta = math.sin(theta_rad)
    
    x = v[0] * cos_theta + v[2] * sin_theta
    y = v[1]
    z = -v[0] * sin_theta + v[2] * cos_theta
    
    return (x, y, z)


def normalize_vector(v):
    """Normalize vector to unit length."""
    magnitude = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    if magnitude == 0:
        return (0, 0, 0)
    return (v[0] / magnitude, v[1] / magnitude, v[2] / magnitude)


def apply_rotation_pipeline(raw_x, raw_y, raw_z, rho, theta):
    """
    Apply the complete rotation pipeline to raw accelerometer data.
    
    Args:
        raw_x, raw_y, raw_z: Raw accelerometer values in milli-g
        rho: Rotation angle around X axis (degrees)
        theta: Rotation angle around Y axis (degrees)
    
    Returns:
        Tuple of (input_vec, rotated_vec, normalized_vec, scalar_projection)
    """
    # Stage 0: Convert milli-g to g
    input_vec = (raw_x / 1000.0, raw_y / 1000.0, raw_z / 1000.0)
    
    # Stage 1: Apply rotation (first X, then Y)
    rotated_x = rotate_vector_x(input_vec, rho)
    rotated_vec = rotate_vector_y(rotated_x, theta)
    
    # Stage 2: Normalize to unit vector
    normalized_vec = normalize_vector(rotated_vec)
    
    # Stage 3: Scalar projection (X component)
    scalar_projection = normalized_vec[0]
    
    return input_vec, rotated_vec, normalized_vec, scalar_projection


def apply_linear_function(scalar, scale, offset):
    """Apply linear conversion function.
    Per spec: MIDI = clamp((scalar * scale + offset) * 127, 0, 127)
    """
    result = (scalar * scale + offset) * 127.0
    return max(0, min(127, int(round(result))))


def apply_exponential_function(scalar, exponent):
    """Apply exponential conversion function."""
    # Formula: ((scalar + 1) / 2)^exponent * 127
    normalized = (scalar + 1.0) / 2.0  # Convert [-1, 1] to [0, 1]
    result = (normalized ** exponent) * 127.0
    return max(0, min(127, int(round(result))))


def apply_scurve_function(scalar, steepness):
    """Apply S-curve (sigmoid) conversion function."""
    # Formula: 127 / (1 + e^(-steepness * scalar))
    result = 127.0 / (1.0 + math.exp(-steepness * scalar))
    return max(0, min(127, int(round(result))))


def apply_lookup_function(scalar, points):
    """Apply lookup table with linear interpolation."""
    # Input points are fixed at: [-1.0, -0.5, 0.0, 0.5, 1.0]
    input_points = [-1.0, -0.5, 0.0, 0.5, 1.0]
    
    # Clamp scalar to valid range
    scalar = max(-1.0, min(1.0, scalar))
    
    # Find which segment the scalar falls into
    for i in range(len(input_points) - 1):
        if scalar >= input_points[i] and scalar <= input_points[i + 1]:
            # Linear interpolation between points[i] and points[i+1]
            t = (scalar - input_points[i]) / (input_points[i + 1] - input_points[i])
            result = points[i] + t * (points[i + 1] - points[i])
            return max(0, min(127, int(round(result))))
    
    # Edge case: exactly at last point
    return max(0, min(127, int(round(points[-1]))))


def calculate_expected_midi(scalar, func_type, func_params):
    """Calculate expected MIDI value based on function type and parameters."""
    if func_type == "linear":
        scale, offset = func_params
        return apply_linear_function(scalar, scale, offset)
    elif func_type == "exponential":
        exponent = func_params[0]
        return apply_exponential_function(scalar, exponent)
    elif func_type == "scurve":
        steepness = func_params[0]
        return apply_scurve_function(scalar, steepness)
    elif func_type == "lookup":
        return apply_lookup_function(scalar, func_params)
    else:
        raise ValueError(f"Unknown function type: {func_type}")


def validate_pipeline_stage(monitor_data, expected, rho, theta, func_type, func_params):
    """
    Validate monitor output against expected pipeline calculations.
    
    Returns: (success, error_messages)
    """
    errors = []
    
    # Extract monitor data
    raw_x = monitor_data['raw_axis']['x']
    raw_y = monitor_data['raw_axis']['y']
    raw_z = monitor_data['raw_axis']['z']
    
    # Calculate expected values
    input_vec, rotated_vec, normalized_vec, scalar = apply_rotation_pipeline(
        raw_x, raw_y, raw_z, rho, theta
    )
    
    expected_midi = calculate_expected_midi(scalar, func_type, func_params)
    
    # Validate input vector (should just be raw / 1000)
    actual_input = (
        monitor_data['input_vector']['x'],
        monitor_data['input_vector']['y'],
        monitor_data['input_vector']['z']
    )
    
    for i, axis in enumerate(['x', 'y', 'z']):
        if abs(actual_input[i] - input_vec[i]) > TOLERANCE_VECTOR:
            errors.append(
                f"Input vector {axis}: expected {input_vec[i]:.3f}, "
                f"got {actual_input[i]:.3f} (diff: {abs(actual_input[i] - input_vec[i]):.3f})"
            )
    
    # Validate rotated vector
    actual_rotated = (
        monitor_data['rotated_vector']['x'],
        monitor_data['rotated_vector']['y'],
        monitor_data['rotated_vector']['z']
    )
    
    for i, axis in enumerate(['x', 'y', 'z']):
        if abs(actual_rotated[i] - rotated_vec[i]) > TOLERANCE_VECTOR:
            errors.append(
                f"Rotated vector {axis}: expected {rotated_vec[i]:.3f}, "
                f"got {actual_rotated[i]:.3f} (diff: {abs(actual_rotated[i] - rotated_vec[i]):.3f})"
            )
    
    # Validate normalized vector
    actual_normalized = (
        monitor_data['normalized_vector']['x'],
        monitor_data['normalized_vector']['y'],
        monitor_data['normalized_vector']['z']
    )
    
    for i, axis in enumerate(['x', 'y', 'z']):
        if abs(actual_normalized[i] - normalized_vec[i]) > TOLERANCE_VECTOR:
            errors.append(
                f"Normalized vector {axis}: expected {normalized_vec[i]:.3f}, "
                f"got {actual_normalized[i]:.3f} (diff: {abs(actual_normalized[i] - normalized_vec[i]):.3f})"
            )
    
    # Validate scalar projection
    actual_scalar = monitor_data['scalar_projection']
    if abs(actual_scalar - scalar) > TOLERANCE_SCALAR:
        errors.append(
            f"Scalar projection: expected {scalar:.3f}, "
            f"got {actual_scalar:.3f} (diff: {abs(actual_scalar - scalar):.3f})"
        )
    
    # Validate MIDI value
    actual_midi = monitor_data['midi_output']['value']
    if abs(actual_midi - expected_midi) > TOLERANCE_MIDI:
        errors.append(
            f"MIDI value: expected {expected_midi}, "
            f"got {actual_midi} (diff: {abs(actual_midi - expected_midi)})"
        )
    
    return (len(errors) == 0, errors)


def test_pipeline_configuration(ser, config_name, rho, theta, cc, func_type, func_params):
    """
    Test a specific pipeline configuration.
    
    Args:
        config_name: Descriptive name for this test case
        rho: Rotation around X axis (degrees)
        theta: Rotation around Y axis (degrees)
        cc: MIDI CC number
        func_type: "linear", "exponential", "scurve", or "lookup"
        func_params: List of function parameters
    
    Returns: (success, error_messages)
    """
    print(f"\n{'=' * 70}")
    print(f"Testing: {config_name}")
    print(f"  Rho: {rho}°, Theta: {theta}°, CC: {cc}")
    print(f"  Function: {func_type}, Params: {func_params}")
    print(f"{'=' * 70}\n")
    
    # Build pipeline set command
    params_str = " ".join(str(p) for p in func_params)
    cmd = f"pipeline set {rho} {theta} {cc} {func_type} {params_str}"
    
    # Configure pipeline
    response = send_command(ser, cmd, wait_time=0.8)
    
    # Verify configuration was accepted
    if "error" in response.lower() or "invalid" in response.lower():
        return (False, [f"Pipeline configuration rejected: {response}"])
    
    # Prompt user to verify device is stable
    print("\n" + "=" * 70)
    print("USER ACTION:")
    print("Ensure the Thingy:53 device is in a STABLE position.")
    print("DO NOT move the device between tests - keep it in the same position.")
    print("The device should be connected via BLE and sending accelerometer data.")
    print("=" * 70)
    input("Press Enter to continue...")
    
    # Wait for configuration to take effect and for fresh accelerometer data
    print("  Waiting for configuration to apply and fresh data to arrive...")
    time.sleep(2.0)  # Give time for config reload and new accel data
    
    # Read monitor data multiple times until we get data with the correct configuration
    monitor_data = None
    func_type_names = {"linear": "linear", "exponential": "exponential", "scurve": "scurve", "lookup": "lookup"}
    expected_func = func_type_names[func_type]
    
    for attempt in range(10):  # Try up to 10 times over 5 seconds
        ser.reset_input_buffer()  # Flush old data
        time.sleep(0.3)  # Wait for fresh data
        
        response = send_command(ser, "monitor json", wait_time=0.5, verbose=False)
        
        try:
            # Extract JSON from response
            json_start = response.find('{')
            json_end = response.rfind('}') + 1
            if json_start >= 0 and json_end > json_start:
                json_str = response[json_start:json_end]
                data = json.loads(json_str)
                
                # Check if this data reflects our new configuration
                if (data['function_type'] == expected_func and 
                    data['midi_output']['cc'] == cc):
                    monitor_data = data
                    print(f"  Configuration applied (attempt {attempt + 1})")
                    break
                else:
                    print(f"  Attempt {attempt + 1}: Waiting for config to apply "
                          f"(got {data['function_type']}/CC{data['midi_output']['cc']}, "
                          f"want {expected_func}/CC{cc})")
        except json.JSONDecodeError as e:
            print(f"  Attempt {attempt + 1}: Failed to parse JSON - {e}")
    
    if monitor_data is None:
        return (False, [f"Failed to get monitor data with correct configuration after 10 attempts. "
                        f"Expected function={expected_func}, CC={cc}"])
    
    # Display monitor data
    print("\nMonitor Data:")
    print(f"  Raw axis: x={monitor_data['raw_axis']['x']}, "
          f"y={monitor_data['raw_axis']['y']}, z={monitor_data['raw_axis']['z']}")
    print(f"  Function: {monitor_data['function_type']}")
    print(f"  MIDI output: CC {monitor_data['midi_output']['cc']} = {monitor_data['midi_output']['value']}")
    
    # Validate against expected values
    success, errors = validate_pipeline_stage(monitor_data, None, rho, theta, func_type, func_params)
    
    if success:
        print(f"\n✓ {config_name}: PASSED")
    else:
        print(f"\n✗ {config_name}: FAILED")
        for error in errors:
            print(f"    {error}")
    
    return (success, errors)


def main():
    parser = argparse.ArgumentParser(description='Validate accelerometer pipeline calculations')
    parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
    parser.add_argument('--inject-data', help='[Future] Inject test data instead of using real device')
    args = parser.parse_args()
    
    port = args.port if args.port else select_port(auto_select=True)
    if port is None:
        print(f"TEST FAILED: {TEST_NAME} - No port selected")
        sys.exit(1)
    
    if args.inject_data:
        print("Note: --inject-data flag recognized but not yet supported by firmware")
        print("Continuing with interactive test (requires connected Thingy:53)")
        time.sleep(2)
    
    try:
        print(f"Opening {port}...")
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        print(f"Connected to {port}")
        time.sleep(1)
        ser.reset_input_buffer()
        
        print("\n" + "=" * 70)
        print("ACCELEROMETER PIPELINE VALIDATION TEST")
        print("=" * 70)
        print("\nThis test validates the complete accelerometer processing pipeline:")
        print("  1. Rotation matrix calculations (rho/theta)")
        print("  2. Vector normalization")
        print("  3. Scalar projection")
        print("  4. Conversion functions (linear, exponential, s-curve)")
        print("  5. MIDI value generation")
        print("\nIMPORTANT: Place the device in a stable position at the start.")
        print("DO NOT move the device between tests - it should remain stationary.")
        print("The test will configure different pipeline settings and validate")
        print("that the firmware correctly processes the same raw accelerometer data.")
        print("=" * 70)
        
        # Test cases
        test_cases = [
            # (name, rho, theta, cc, func_type, func_params)
            ("No rotation, linear 1:1", 0, 0, 1, "linear", [1.0, 0.0]),
            ("45° X rotation, linear", 45, 0, 2, "linear", [1.0, 0.0]),
            ("90° Y rotation, linear", 0, 90, 3, "linear", [1.0, 0.0]),
            ("45° X + 45° Y, linear", 45, 45, 4, "linear", [1.0, 0.0]),
            ("No rotation, exponential 2.0", 0, 0, 5, "exponential", [2.0]),
            ("No rotation, exponential 0.5", 0, 0, 6, "exponential", [0.5]),
            ("No rotation, s-curve 5.0", 0, 0, 7, "scurve", [5.0]),
            ("No rotation, s-curve 10.0", 0, 0, 8, "scurve", [10.0]),
        ]
        
        results = []
        
        for test_case in test_cases:
            name, rho, theta, cc, func_type, func_params = test_case
            success, errors = test_pipeline_configuration(
                ser, name, rho, theta, cc, func_type, func_params
            )
            results.append((name, success, errors))
            
            # Pause between tests
            if test_case != test_cases[-1]:
                print("\nPreparing for next test...")
                time.sleep(2)
        
        # Summary
        print("\n" + "=" * 70)
        print("TEST SUMMARY")
        print("=" * 70)
        
        passed_count = sum(1 for _, success, _ in results if success)
        total_count = len(results)
        
        for name, success, errors in results:
            status = "✓ PASS" if success else "✗ FAIL"
            print(f"  {status}: {name}")
            if not success and errors:
                for error in errors[:2]:  # Show first 2 errors
                    print(f"      {error}")
        
        print(f"\nPassed: {passed_count}/{total_count}")
        print("=" * 70)
        
        ser.close()
        
        if passed_count == total_count:
            print(f"\nTEST PASSED: {TEST_NAME}")
            sys.exit(0)
        else:
            print(f"\nTEST FAILED: {TEST_NAME} - {total_count - passed_count} test case(s) failed")
            sys.exit(1)
            
    except Exception as e:
        print(f"\nTEST FAILED: {TEST_NAME} - {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
