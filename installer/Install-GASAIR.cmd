@echo off
setlocal
title GASAIR 1.0.3 - Gassymixing
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-GASAIR.ps1"
set "installResult=%errorlevel%"
echo.
pause
exit /b %installResult%
