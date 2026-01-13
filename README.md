# GuitarAcc - BLE Guitar Accessory System

A BLE-based guitar accessory system built with Nordic nRF Connect SDK.

## Project Structure

```
guitaracc/
├── basestation/         # Basestation application (nRF5340 Audio DK)
│   ├── src/
│   │   └── main.c
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── Kconfig
├── client/              # Client application (Thingy:53)
│   ├── src/
│   │   └── main.c
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── Kconfig
└── README.md
```

## Hardware

- **Basestation**: nRF5340 Audio DK (`nrf5340_audio_dk_nrf5340_cpuapp`)
- **Client**: Thingy:53 (`thingy53_nrf5340_cpuapp`)

## Prerequisites

- nRF Connect for VS Code extension
- nRF Connect SDK installed
- nRF Command Line Tools

## Getting Started

### Building

1. Open this workspace in VS Code with nRF Connect extension
2. In the nRF Connect panel, click "Add build configuration"
3. Select the application (basestation or client)
4. Choose the appropriate board target:
   - Basestation: `nrf5340_audio_dk_nrf5340_cpuapp`
   - Client: `thingy53_nrf5340_cpuapp`
5. Click "Build"

### Flashing

1. Connect your device via USB
2. In the nRF Connect panel, select your build configuration
3. Click "Flash"

### Debugging

1. Select your build configuration
2. Click "Debug" in the nRF Connect panel

## Current Features

### Basestation
- BLE peripheral role
- Advertising as "GuitarAcc Base"
- Connection handling

### Client
- BLE central role
- Device scanning
- Connection handling

## License

Apache-2.0
