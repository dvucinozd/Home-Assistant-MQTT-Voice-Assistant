# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.1] - 2025-12-22

### Added
- Initial release of JC-ESP32P4-M3-DEV Voice Assistant firmware
- WakeNet9 wake-word detection (wn9_hiesp model, "Hi ESP")
- Home Assistant Assist pipeline integration via WebSocket
- Voice Activity Detection (VAD) with adjustable thresholds
- Local timer fallback with Croatian language support (timer/tajmer/odbrojavanje)
- MP3 player with SD card support for local music playback
- MQTT Home Assistant Discovery with 20+ entities (sensors, switches, numbers, buttons, text)
- OTA updates via HTTP with progress tracking and rollback support
- Web dashboard at http://<device-ip>/ with real-time API endpoints
- WebSerial logging interface at http://<device-ip>/webserial
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
- Full HA Assist pipeline: STT → Intent → TTS
- Adjustable wake-word detection threshold (0.50-0.95)
- Dual audio modes: voice pipeline and local music playback
- MQTT control of device parameters in real-time
- Secure OTA with HTTP status validation
- Extensible architecture for custom intents
- Windows, Linux, and macOS build support
- Helper scripts for HA WebSocket diagnostics

### Configuration
- Template-based configuration (config.h.example → config.h)
- NVS persistent storage for runtime settings
- Menuconfig support for ESP-IDF sdkconfig
- Safe defaults for all major parameters

### Documentation
- Comprehensive README with hardware/software setup
- Project structure overview and file descriptions
- LED status reference table
- MQTT entity reference
- Troubleshooting guide
- Technical specifications document
- Quick Start for Windows, Linux, macOS

### Known Limitations
- Single language support (Croatian/English in templates)
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

Please report any issues or bugs on the [GitHub Issues](https://github.com/dvucinozd/JC-ESP32P4-M3-DEV-Voice-Assistant-V2/issues) page.
Use the provided bug report template for best results.