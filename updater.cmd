@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0updater.ps1" %*
exit /b %ERRORLEVEL%
