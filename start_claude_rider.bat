@echo off
chcp 65001 >nul 2>&1
title Claude Rider Setup

echo.
echo  ========================================
echo           CLAUDE RIDER v3.0
echo     LED Status fuer Claude Code
echo  ========================================
echo.

:: Python pruefen
python --version >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] Python nicht gefunden. Bitte installieren: https://python.org
    pause
    exit /b 1
)

:: pyserial installieren falls noetig
python -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installiere pyserial...
    pip install pyserial
    if errorlevel 1 (
        echo [FEHLER] pyserial konnte nicht installiert werden.
        pause
        exit /b 1
    )
    echo [OK] pyserial installiert.
)

:: Pruefen ob Daemon schon laeuft
python "%~dp0python\claude_rider.py" --status >nul 2>&1
if not errorlevel 1 (
    echo [INFO] Claude Rider Daemon laeuft bereits.
    python "%~dp0python\claude_rider.py" --status
    pause
    exit /b 0
)

:: Daemon starten
echo [INFO] Starte Claude Rider Daemon...
start /b "" python "%~dp0python\claude_rider.py" --daemon
timeout /t 3 /nobreak >nul

:: Pruefen ob Daemon gestartet ist
python "%~dp0python\claude_rider.py" --status
if errorlevel 1 (
    echo [FEHLER] Daemon konnte nicht gestartet werden.
    echo          Ist die LED-Leiste per USB angeschlossen?
    pause
    exit /b 1
)

echo.
echo [OK] Claude Rider laeuft!
echo.
pause
