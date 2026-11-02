@echo off
setlocal
cd /d "%~dp0"
if not exist "Binaries\MidnightNative\MidnightNativePOC.exe" (
  echo Build the application first with Build-Midnight-Native-POC.cmd.
  pause
  exit /b 2
)
start "Midnight Native V0.1" /d "%~dp0Binaries\MidnightNative" "%~dp0Binaries\MidnightNative\MidnightNativePOC.exe" %*
