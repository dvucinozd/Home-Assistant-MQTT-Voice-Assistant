@echo off
REM ESP32-P4 Voice Assistant Flash Script

echo ========================================
echo ESP32-P4 Voice Assistant Flash
echo ========================================

set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5
call %IDF_PATH%\export.bat

cd /d D:\platformio\P4\esp32-p4-voice-assistant

REM Flash to board
echo.
echo Flashing to board on COM13 (USB/JTAG)...
echo Board: JC-ESP32P4-M3-DEV
echo.
idf.py -p COM13 flash monitor

pause
