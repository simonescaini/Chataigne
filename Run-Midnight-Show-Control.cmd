@echo off
setlocal
cd /d "%~dp0"
if not exist "Binaries\MidnightShowControl\MidnightShowControl.exe" (
  echo Integration build is missing. Run Build-Midnight-Show-Control.cmd.
  exit /b 2
)
start "Midnight Show Control" /d "%~dp0Binaries\MidnightShowControl" "%~dp0Binaries\MidnightShowControl\MidnightShowControl.exe" %*
