@echo off
chcp 65001 >nul 2>&1
title Claude Rider Setup

echo.
echo  ========================================
echo           CLAUDE RIDER v3.0
echo     LED Status for Claude Code
echo  ========================================
echo.

:: Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found. Please install: https://python.org
    pause
    exit /b 1
)

:: Install pyserial if needed
python -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installing pyserial...
    pip install pyserial
    if errorlevel 1 (
        echo [ERROR] Could not install pyserial.
        pause
        exit /b 1
    )
    echo [OK] pyserial installed.
)

:: Check if daemon is already running
python "%~dp0python\claude_rider.py" --status >nul 2>&1
if not errorlevel 1 (
    echo [INFO] Claude Rider daemon is already running.
    python "%~dp0python\claude_rider.py" --status
    pause
    exit /b 0
)

:: Start daemon
echo [INFO] Starting Claude Rider daemon...
start /b "" python "%~dp0python\claude_rider.py" --daemon
timeout /t 3 /nobreak >nul

:: Verify daemon started
python "%~dp0python\claude_rider.py" --status
if errorlevel 1 (
    echo [ERROR] Could not start daemon.
    echo         Is the LED bar connected via USB?
    pause
    exit /b 1
)

echo.
echo [OK] Claude Rider is running!
echo.
pause
