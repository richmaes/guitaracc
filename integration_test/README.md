# GuitarAcc Integration Test Framework

## Overview

This directory contains a **Software-in-the-Loop (SIL)** integration testing framework that simulates the complete BLE communication between the client (guitar device) and basestation without requiring physical hardware.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Integration Test Harness                     │
│                                                                  │
│  ┌──────────────────────┐         ┌─────────────────────────┐  │
│  │   Client Emulator    │         │  Basestation Emulator   │  │
│  │                      │         │                         │  │
│  │  • Motion logic      │         │  • MIDI logic           │  │
│  │  • Accel simulation  │         │  • Connection handling  │  │
│  │  • BLE peripheral    │         │  • BLE central          │  │
│  └──────────┬───────────┘         └───────────┬─────────────┘  │
│             │                                 │                 │
│             │    ┌─────────────────────┐     │                 │
│             └───→│   Message Queue     │←────┘                 │
│                  │   (BLE Emulation)   │                       │
│                  │                     │                       │
│                  │  • Connection req   │                       │
│                  │  • Notifications    │                       │
│                  │  • GATT operations  │                       │
│                  └─────────────────────┘                       │
└─────────────────────────────────────────────────────────────────┘
```

## Components

### 1. BLE Hardware Abstraction Layer (ble_hal.h/c)
- Provides platform-independent BLE interfaces
- Emulates BLE connection, advertising, GATT operations
- Message queue for packet delivery

### 2. Client Emulator (client_emulator.h/c)
- Uses actual `motion_logic.c` for business logic
- Simulates accelerometer data generation
- BLE peripheral role (advertises, sends notifications)

### 3. Basestation Emulator (basestation_emulator.h/c)
- Uses actual `midi_logic.c` for business logic
- BLE central role (scans, connects, receives notifications)
- MIDI output simulation

### 4. Integration Tests (test_integration.c)
- End-to-end scenarios
- Connection establishment
- Data flow validation
- Complete motion → MIDI pipeline

## Test Scenarios

### Scenario 1: Connection Establishment
1. Client starts advertising
2. Basestation scans and discovers client
3. Basestation connects to client
4. Client accepts connection
5. Basestation enables notifications

### Scenario 2: Data Flow
1. Client generates accelerometer data (or uses test data)
2. Client converts to milli-g using `motion_logic`
3. Client sends BLE notification
4. Basestation receives notification
5. Basestation converts to MIDI using `midi_logic`
6. Verify MIDI output correctness

### Scenario 3: Disconnection and Reconnection
1. Client disconnects
2. Basestation detects disconnection
3. Client re-advertises
4. Basestation reconnects
5. Data flow resumes

### Scenario 4: Multiple Data Updates
1. Send sequence of motion data
2. Verify change detection works (only changed data sent)
3. Verify MIDI output sequence matches input

## Building and Running

```bash
# From project root
make test-integration

# Or from this directory
cd integration_test
make test
```

## Benefits

1. **No Hardware Required** - Test complete system on development machine
2. **Faster Iteration** - No flashing, instant feedback
3. **Deterministic** - Reproducible test scenarios
4. **CI/CD Ready** - Runs in automated pipelines
5. **Single Source of Truth** - Uses actual business logic code
6. **Coverage** - Test scenarios impossible/difficult with real hardware

## Implementation Status

- [ ] BLE Hardware Abstraction Layer
- [ ] Message Queue System
- [ ] Client Emulator
- [ ] Basestation Emulator
- [ ] Integration Test Suite
- [ ] Build System Integration

## Future Enhancements

- Timing simulation (connection intervals, latency)
- Error injection (packet loss, corruption)
- Multiple client support (up to 4 guitars)
- Performance profiling
- Coverage analysis
