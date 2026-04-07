@echo off
chcp 65001 >nul 2>&1
title Claude Rider Stop

echo.
echo [INFO] Shutting down Claude Rider...

python "%~dp0python\claude_rider.py" --stop

echo [OK] Claude Rider stopped.
echo.
pause
