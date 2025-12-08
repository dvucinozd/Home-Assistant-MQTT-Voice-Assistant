@echo off
echo ========================================
echo   ESP32-P4 OTA Update Server
echo ========================================
echo.

cd /d "%~dp0build"

echo Starting HTTP server on port 8080...
echo.
echo Firmware file: esp32-p4-voice-assistant.bin
echo.

for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /c:"IPv4"') do (
    set IP=%%a
    goto :found
)
:found
set IP=%IP:~1%

echo ========================================
echo   OTA URL:
echo   http://%IP%:8080/esp32-p4-voice-assistant.bin
echo ========================================
echo.
echo Press Ctrl+C to stop the server.
echo.

python -m http.server 8080
