# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.1] - 2025-12-22

### Added
- Initial release of JC-ESP32P4-M3-DEV Voice Assistant firmware
- WakeNet9 wake-word detection (`wn9_hiesp` model, "Hi ESP")
- Home Assistant Assist pipeline integration via WebSocket
- Voice Activity Detection (VAD) with adjustable thresholds
- Local timer fallback with Croatian keywords support ("timer/tajmer/odbrojavanje")
- MP3 player with SD card support for local music playback
- MQTT Home Assistant Discovery with 20+ entities (sensors, switches, numbers, buttons, text)
- OTA updates via HTTP with progress tracking and rollback support
- Web dashboard at `http://<device-ip>/` with API endpoints
- WebSerial logging interface at `http://<device-ip>/webserial`
- RGB LED status indicators (8 states: BOOTING, IDLE, LISTENING, PROCESSING, SPEAKING, OTA, ERROR, CONNECTING)
- Optional OLED status display (SSD1306 128x64, I2C)
- Safe Mode boot-loop protection with watchdog
- Reset diagnostics and system health monitoring
- ES8311 audio codec support (microphone + speaker)
- ESP32-C6 Wi-Fi coprocessor integration over SDIO
- Configurable AGC (Automatic Gain Control) and volume control
- NVS-based persistent configuration (fallback to config.h template)

### Features
- Offline wake-word detection (no cloud dependency for wake word)
- Full HA Assist pipeline: STT -> Intent -> TTS
- Adjustable wake-word detection threshold (0.50-0.95)
- Dual audio modes: voice pipeline and local music playback
- MQTT control of device parameters in real-time
- OTA with HTTP status validation (and support for unknown `Content-Length`)
- Cross-platform build scripts (Python + shell), plus Windows helper scripts
- Helper scripts for HA WebSocket diagnostics

### Configuration
- Template-based configuration (`main/config.h.example` -> `main/config.h`)
- NVS persistent storage for runtime settings
- Menuconfig support for ESP-IDF `sdkconfig`
- Safe defaults for all major parameters

### Documentation
- README with hardware/software setup and MQTT entity overview
- Technical specifications: `docs/TECHNICAL_SPECIFICATIONS.md`
- WakeNet model setup (flash/SD): `docs/WAKENET_SD_CARD_SETUP.md`
- OLED status notes/TODO: `docs/TODO_OLED_STATUS_DISPLAY.md`

### Known Limitations
- Requires ESP-IDF v5.5
- JC-ESP32P4-M3-DEV board specifically tested (may work on variants)
- OLED display is optional (not required for core functionality)

---

## Future Roadmap

- [ ] Multi-language support (wake words, timers)
- [ ] Additional board support (generic ESP32-P4 setups)
- [ ] Bluetooth speaker pairing
- [ ] Local LLM integration
- [ ] Enhanced diagnostics dashboard

---

## How to Report Issues

Please report issues/bugs on the GitHub Issues page:
https://github.com/dvucinozd/JC-ESP32P4-M3-DEV-Voice-Assistant-V2/issues

Use the provided bug report template for best results.
