#!/usr/bin/env python3
"""
Configuration Import/Export Tool for GuitarAcc Basestation

This tool facilitates backup, restore, and remote configuration management
of the basestation via JSON files.
"""

import serial
import time
import sys
import json
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


def export_config(port, export_type='full', patch_num=None):
    """Export configuration from device."""
    try:
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        print(f"Connected to {port}")
        time.sleep(1)
        
        # Build export command
        if export_type == 'global':
            cmd = "config export global"
        elif export_type == 'patch' and patch_num is not None:
            cmd = f"config export patch {patch_num}"
        else:
            cmd = "config export"
        
        print(f"Executing: {cmd}")
        response = send_command(ser, cmd, wait_time=3.0)
        
        ser.close()
        
        # Extract JSON from response - remove ANSI codes and find JSON boundaries
        import re
        ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        response = ansi_escape.sub('', response)
        
        # Find JSON object boundaries
        start_idx = response.find('{')
        if start_idx == -1:
            print("No JSON object found in response", file=sys.stderr)
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
        
        json_text = response[start_idx:end_idx]
        
        # Validate JSON
        try:
            config_data = json.loads(json_text)
            return config_data
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON: {e}", file=sys.stderr)
            print(f"Extracted text:\n{json_text}", file=sys.stderr)
            return None
            
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return None


def import_config(port, config_file):
    """Import configuration to device."""
    try:
        # Read and validate JSON file
        with open(config_file, 'r') as f:
            config_data = json.load(f)
        
        print(f"Loaded configuration from {config_file}")
        print(f"Configuration version: {config_data.get('version', 'unknown')}")
        
        # Determine what's being imported
        config_section = config_data.get('config', {})
        has_global = 'global' in config_section
        has_patches = 'patches' in config_section
        
        if has_global and has_patches:
            print("Importing: Full configuration (global + all patches)")
        elif has_global:
            print("Importing: Global configuration only")
        elif has_patches:
            patch_nums = [p.get('patch_num', '?') for p in config_section['patches']]
            print(f"Importing: Patches {patch_nums}")
        
        # Note: Interactive import via shell is complex
        # For now, provide instructions for manual paste
        print("\n" + "="*70)
        print("IMPORT INSTRUCTIONS")
        print("="*70)
        print("1. Connect to device serial port")
        print(f"2. Type: config import")
        print("3. Paste the following JSON:")
        print("="*70)
        print(json.dumps(config_data, indent=2))
        print("="*70)
        print("4. Press Ctrl+D or send terminator")
        print("\nNote: Automated import via serial will be implemented in future version")
        
        return True
        
    except FileNotFoundError:
        print(f"Error: File '{config_file}' not found", file=sys.stderr)
        return False
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{config_file}': {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return False


def validate_config(config_file):
    """Validate configuration JSON file."""
    try:
        with open(config_file, 'r') as f:
            config_data = json.load(f)
        
        # Check required fields
        if 'version' not in config_data:
            print("❌ Missing required field: 'version'")
            return False
        
        if 'config' not in config_data:
            print("❌ Missing required field: 'config'")
            return False
        
        config_section = config_data['config']
        
        # Validate global section if present
        if 'global' in config_section:
            global_cfg = config_section['global']
            print("✓ Global configuration present")
            
            # Check ranges
            if 'midi_channel' in global_cfg:
                val = global_cfg['midi_channel']
                if not (0 <= val <= 15):
                    print(f"❌ midi_channel out of range: {val} (valid: 0-15)")
                    return False
            
            if 'running_average_depth' in global_cfg:
                val = global_cfg['running_average_depth']
                if not (3 <= val <= 10):
                    print(f"❌ running_average_depth out of range: {val} (valid: 3-10)")
                    return False
        
        # Validate patches section if present
        if 'patches' in config_section:
            patches = config_section['patches']
            print(f"✓ {len(patches)} patch(es) present")
            
            for i, patch in enumerate(patches):
                patch_num = patch.get('patch_num', i)
                
                if not (0 <= patch_num <= 15):
                    print(f"❌ Patch {i}: Invalid patch_num {patch_num} (valid: 0-15)")
                    return False
                
                # Validate CC mappings
                if 'cc_mapping' in patch:
                    cc_map = patch['cc_mapping']
                    if len(cc_map) != 6:
                        print(f"❌ Patch {patch_num}: cc_mapping must have 6 values")
                        return False
                    for j, val in enumerate(cc_map):
                        if not (0 <= val <= 127):
                            print(f"❌ Patch {patch_num}: cc_mapping[{j}] = {val} (valid: 0-127)")
                            return False
                
                # Validate accel_min/max
                for field in ['accel_min', 'accel_max']:
                    if field in patch:
                        arr = patch[field]
                        if len(arr) != 6:
                            print(f"❌ Patch {patch_num}: {field} must have 6 values")
                            return False
                        for j, val in enumerate(arr):
                            if not (0 <= val <= 127):
                                print(f"❌ Patch {patch_num}: {field}[{j}] = {val} (valid: 0-127)")
                                return False
        
        print("\n✅ Configuration file is valid")
        return True
        
    except FileNotFoundError:
        print(f"❌ File '{config_file}' not found", file=sys.stderr)
        return False
    except json.JSONDecodeError as e:
        print(f"❌ Invalid JSON: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f'❌ Error: {e}', file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description='GuitarAcc Basestation Configuration Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Export full configuration to file
  %(prog)s export -p /dev/ttyUSB0 -o config.json
  
  # Export only global settings
  %(prog)s export -p /dev/ttyUSB0 -t global -o global.json
  
  # Export single patch
  %(prog)s export -p /dev/ttyUSB0 -t patch --patch 5 -o patch5.json
  
  # Import configuration
  %(prog)s import -p /dev/ttyUSB0 -i config.json
  
  # Validate configuration file
  %(prog)s validate -i config.json
"""
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Export command
    export_parser = subparsers.add_parser('export', help='Export configuration from device')
    export_parser.add_argument('-p', '--port', help='Serial port (or use auto-select)')
    export_parser.add_argument('-t', '--type', choices=['full', 'global', 'patch'], 
                               default='full', help='Export type')
    export_parser.add_argument('--patch', type=int, help='Patch number for patch export (0-15)')
    export_parser.add_argument('-o', '--output', required=True, help='Output JSON file')
    
    # Import command
    import_parser = subparsers.add_parser('import', help='Import configuration to device')
    import_parser.add_argument('-p', '--port', help='Serial port (or use auto-select)')
    import_parser.add_argument('-i', '--input', required=True, help='Input JSON file')
    
    # Validate command
    validate_parser = subparsers.add_parser('validate', help='Validate configuration file')
    validate_parser.add_argument('-i', '--input', required=True, help='Input JSON file')
    
    args = parser.parse_args()
    
    if args.command == 'export':
        # Get port
        if args.port:
            port = args.port
        else:
            port = select_port(auto_select=True)
            if port is None:
                print("No port selected. Exiting.", file=sys.stderr)
                sys.exit(1)
        
        # Validate patch number for patch export
        if args.type == 'patch' and args.patch is None:
            print("Error: --patch required for patch export", file=sys.stderr)
            sys.exit(1)
        
        # Export configuration
        config_data = export_config(port, args.type, args.patch)
        if config_data is None:
            sys.exit(1)
        
        # Write to file
        with open(args.output, 'w') as f:
            json.dump(config_data, f, indent=2)
        
        print(f"\n✅ Configuration exported to {args.output}")
        
    elif args.command == 'import':
        # Get port
        if args.port:
            port = args.port
        else:
            port = select_port(auto_select=True)
            if port is None:
                print("No port selected. Exiting.", file=sys.stderr)
                sys.exit(1)
        
        # Import configuration
        if not import_config(port, args.input):
            sys.exit(1)
        
    elif args.command == 'validate':
        if not validate_config(args.input):
            sys.exit(1)
    
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
