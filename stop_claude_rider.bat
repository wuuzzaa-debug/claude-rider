@echo off
chcp 65001 >nul 2>&1
title Claude Rider Stop

echo.
echo [INFO] Fahre Claude Rider herunter...

python "%~dp0python\claude_rider.py" --stop

echo [OK] Claude Rider gestoppt.
echo.
pause
