# GuitarAcc Project - Nordic nRF Connect SDK

## Project Overview
BLE-based guitar accessory system with two applications:
- **Basestation**: nRF5340 Audio DK (nrf5340_audio_dk_nrf5340_cpuapp)
- **Client**: Thingy:53 (thingy53_nrf5340_cpuapp)

## Development Environment
- Nordic nRF Connect SDK
- Zephyr RTOS
- nRF Connect for VS Code extension

## Board Targets
- Basestation: `nrf5340_audio_dk_nrf5340_cpuapp`
- Client: `thingy53_nrf5340_cpuapp`

## Important Context Files
- **ARCHITECTURE.md**: System architecture, BLE roles, and data flow concepts. Always reference this file when working on BLE functionality or system design.
- **CLI.md**: Command-line interface overview and usage. Reference this when working on shell commands, configuration interface, or serial communication.
- **docs/specs/**: Protocol specifications shared with GUI and external applications. This is the source of truth for communication protocols:
  - **ble-protocol.md**: BLE service/characteristic definitions
  - **midi-commands.md**: MIDI message formats and mapping specifications
  - **serial-protocol.md**: USB/Serial command interface
