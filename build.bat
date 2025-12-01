@echo off
REM ESP32-P4 Voice Assistant Build Script
REM Setup ESP-IDF environment and build project

echo ========================================
echo ESP32-P4 Voice Assistant Build
echo ========================================

REM Set ESP-IDF path
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5

REM Export ESP-IDF environment
call %IDF_PATH%\export.bat

REM Navigate to project directory
cd /d D:\platformio\P4\esp32-p4-voice-assistant

REM Display IDF version
echo.
echo IDF Version:
idf.py --version

REM Build project
echo.
echo Building project...
idf.py build

echo.
echo ========================================
echo Build complete!
echo ========================================
pause
