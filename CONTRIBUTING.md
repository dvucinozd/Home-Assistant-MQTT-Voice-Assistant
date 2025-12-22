# Contributing to JC-ESP32P4-M3-DEV Voice Assistant

Thank you for your interest in contributing! Please follow these guidelines.

## Development Setup

### Prerequisites
- ESP-IDF v5.5.x installed and activated
- Python 3.9+
- Git

### Setup Steps
1. Clone the repository
2. Activate ESP-IDF environment:
   ```bash
   source $IDF_PATH/export.sh  # Linux/macOS
   # or
   %IDF_PATH%\export.bat       # Windows
   ```
3. Build the project:
   ```bash
   python build.py
   ```
4. Flash to your JC-ESP32P4-M3-DEV board:
   ```bash
   python flash.py -p COM13
   ```

## Before Submitting Changes

1. **Update version** in `CMakeLists.txt` (`PROJECT_VER`) if adding new features
2. **Test thoroughly** on JC-ESP32P4-M3-DEV hardware
3. **Update docs**:
   - `README.md` if adding new MQTT entities or user-facing behavior
   - `docs/` if changing WakeNet/OLED/technical details
4. **Check serial logs** for any warnings or errors
5. **Add CHANGELOG entry** in `CHANGELOG.md`
6. **Never commit secrets**: `main/config.h` must stay local (it is in `.gitignore`)

## Code Style

- Follow ESP-IDF coding standards (C99)
- Use meaningful variable names
- Add comments for complex logic
- Keep functions focused and modular

## Reporting Bugs

- Use the bug report template when creating issues
- Include serial log output (first 50 lines minimum)
- Describe reproduction steps clearly
- Specify board version and IDF version

## Feature Requests

- Clearly describe the feature
- Explain the use case
- Consider performance impact
- Discuss backwards compatibility

## Pull Request Process

1. Create a feature branch: `git checkout -b feature/your-feature`
2. Make your changes
3. Test thoroughly
4. Commit with clear messages
5. Push to your fork
6. Create a Pull Request with description

## Questions?

Feel free to open a Discussion or Issue if you have questions!
