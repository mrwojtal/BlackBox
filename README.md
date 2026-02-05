# BlackBox — Raspberry Pi Pico 2W OBD-II Data Logger

A vehicle data logger application for **Raspberry Pi Pico 2W**, written in **C**, that connects to a car’s OBD-II diagnostic port using an **ELM327-compatible interface**, logs real-time telemetry to an SD card, and uses Bluetooth for communication.

Application uses btstack by bluekitchen: https://github.com/bluekitchen/btstack
and no-OS-FatFS-SD-SDIO-SPI-RPi-Pico by carlk3: https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico

---

## Features

- **Live vehicle data logging** via OBD-II (ELM327 compatible adapters)
- Bluetooth communication with ELM327 diagnostic interface
- SD card logging with SPI interface
- Designed for **Raspberry Pi Pico 2W**
- Easily extensible firmware structure via CMake

---

## How It Works

1. **Initialization**
   - On boot, the Pico initializes the Bluetooth stack and SD card interface.

2. **Bluetooth & ELM327 Connection**
   - The device attempts to connect to an ELM327 Bluetooth OBD-II adapter.
   - Once connected, it negotiates communication and protocol for vehicle data.

3. **Data Acquisition**
   - Periodically sends OBD-II PID requests (e.g., RPM, speed, temperature, etc.) to the vehicle’s ECU.
   - Data is parsed and buffered for storage.

4. **Logging**
   - Retrieved sensor values are written to the SD card in a structured log format.

5. **Threading & Performance**
   - Bluetooth handling, data queries, and SD card operations are optimized to avoid blocking the main loop.
