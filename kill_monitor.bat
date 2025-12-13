@echo off
setlocal
chcp 65001>nul

REM Usage:
REM   kill_monitor.bat            (uses ESP32_COM_PORT or COM13)
REM   kill_monitor.bat COM13
REM   kill_monitor.bat --all

set "ARG1=%~1"

set "PORT=%ARG1%"
if /I "%PORT%"=="--all" goto :RUN_ALL
if "%PORT%"=="" set "PORT=%ESP32_COM_PORT%"
if "%PORT%"=="" set "PORT=COM13"

REM Prefer PowerShell 7 (pwsh) if available, fallback to Windows PowerShell.
where pwsh >nul 2>nul
if %ERRORLEVEL%==0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\kill_monitor.ps1" -Port "%PORT%"
  exit /b %ERRORLEVEL%
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\kill_monitor.ps1" -Port "%PORT%"
exit /b %ERRORLEVEL%

:RUN_ALL
where pwsh >nul 2>nul
if %ERRORLEVEL%==0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\kill_monitor.ps1" -All
  exit /b %ERRORLEVEL%
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\kill_monitor.ps1" -All
exit /b %ERRORLEVEL%

