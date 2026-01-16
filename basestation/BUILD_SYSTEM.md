# Global Build System

A top-level Makefile has been created at `/Users/richmaes/src/guitaracc/Makefile` that orchestrates testing and building for the entire GuitarAcc project.

## Key Features

### 1. Test-Driven Build Process
The build system **requires tests to pass** before building firmware:
```
make all → run tests → if pass → build firmware
                    ↓ if fail
                    stop (no build)
```

### 2. Single Command for Everything
```bash
cd /Users/richmaes/src/guitaracc

# Test and build everything
make all

# Or just verify tests
make verify
```

### 3. Individual Control
```bash
# Test individual applications
make test-basestation
make test-client

# Build individual applications  
make build-basestation
make build-client

# Flash individual devices
make flash-basestation
make flash-client
```

## Common Workflows

### Development (Fast Iteration)
```bash
make quick                    # Test + build basestation only (fast!)
make flash-basestation        # Flash to device
make monitor-basestation      # View output
```

### Before Committing
```bash
make verify                   # Run all tests, don't build
```

### Full Build & Deploy
```bash
make all                      # Test + build both applications
make flash                    # Flash both devices
```

### Pristine Rebuild
```bash
make rebuild                  # Clean everything and rebuild
```

### CI/CD Pipeline
```bash
make ci                       # Clean + test + build (returns exit code)
```

## How It Works

### Test Phase
1. Runs `basestation/test/Makefile`
   - Compiles `midi_logic.c` with GCC
   - Runs host-based unit tests
   - If tests fail → **STOP** (no firmware build)

2. Runs `client/test/Makefile` (when implemented)
   - Client tests not yet implemented
   - Currently skips with warning

### Build Phase (only if tests pass)
1. Builds basestation:
   ```bash
   cd basestation
   west build -b nrf5340_audio_dk_nrf5340_cpuapp
   ```

2. Builds client:
   ```bash
   cd client
   west build -b thingy53_nrf5340_cpuapp --sysbuild
   ```

### Output Artifacts
- Basestation: `basestation/build/zephyr/zephyr.hex`
- Client: `client/build/zephyr/merged.hex`

## All Available Targets

Run `make help` from `/Users/richmaes/src/guitaracc/` to see:

**Primary Targets:**
- `all` - Test + build (default)
- `test` - Run all tests
- `build` - Build all firmware (after tests)
- `rebuild` - Pristine rebuild
- `flash` - Flash all devices
- `clean` - Remove all artifacts

**Per-Application:**
- `test-basestation`, `test-client`
- `build-basestation`, `build-client`
- `rebuild-basestation`, `rebuild-client`
- `flash-basestation`, `flash-client`

**Development:**
- `quick` - Fast test + build (basestation only)
- `verify` - Tests only
- `monitor-basestation` - Serial output
- `monitor-client` - Serial output

**CI/CD:**
- `ci` - Full clean build for continuous integration

## Color-Coded Output

The Makefile uses ANSI colors for clear feedback:
- 🟡 **Yellow** - Section headers and warnings
- 🟢 **Green** - Success messages
- 🔴 **Red** - Error messages

## Integration with IDEs

### VS Code
Add to `.vscode/tasks.json`:
```json
{
    "label": "Test & Build",
    "type": "shell",
    "command": "make all",
    "options": {
        "cwd": "${workspaceFolder}/.."
    },
    "group": {
        "kind": "build",
        "isDefault": true
    }
}
```

### Command Line
Just use `make` directly:
```bash
cd /Users/richmaes/src/guitaracc
make <target>
```

## Exit Codes

The Makefile returns proper exit codes for CI/CD:
- **0** - All tests passed, builds successful
- **Non-zero** - Tests failed or build failed

Example CI script:
```bash
#!/bin/bash
cd /path/to/guitaracc
make ci
if [ $? -eq 0 ]; then
    echo "✅ CI passed"
else
    echo "❌ CI failed"
    exit 1
fi
```

## Parallel Builds

To speed up builds, set MAKEFLAGS:
```bash
export MAKEFLAGS="-j8"    # Use 8 parallel jobs
make all
```

## Adding Tests for Client

When you create tests for the client application:

1. Create `client/test/` directory
2. Add test code and Makefile
3. Update the global Makefile's `test-client` target:
   ```makefile
   test-client:
       @echo "Testing Client Application"
       @cd client/test && $(MAKE) test
       @echo "✓ Client tests passed"
   ```

## Advantages

### Before (Manual Process)
```bash
# Developer had to remember:
cd basestation/test && make test    # Test basestation
cd ../../client/test && make test    # Test client (if exists)
cd ../../basestation && west build   # Build basestation
cd ../client && west build           # Build client
cd ../basestation && west flash      # Flash basestation
cd ../client && west flash           # Flash client
```

### After (Automated)
```bash
cd /Users/richmaes/src/guitaracc
make all        # Tests + builds everything
make flash      # Flashes both devices
```

## Summary

The global Makefile provides:
- ✅ **Safety** - Tests must pass before building
- ✅ **Convenience** - Single command builds everything
- ✅ **Flexibility** - Individual or bulk operations
- ✅ **Speed** - `make quick` for fast iteration
- ✅ **CI-Ready** - Proper exit codes and clean builds
- ✅ **Documentation** - Built-in help system

To get started:
```bash
cd /Users/richmaes/src/guitaracc
make help       # See all options
make verify     # Run tests
make all        # Test + build
```
