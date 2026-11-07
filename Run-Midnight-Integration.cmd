@echo off
setlocal
cd /d "%~dp0"
if not exist "Binaries\MidnightIntegration\MidnightIntegration.exe" (
  echo Integration build is missing. Run Build-Midnight-Integration.cmd.
  exit /b 2
)
start "Midnight Chataigne Integration" /d "%~dp0Binaries\MidnightIntegration" "%~dp0Binaries\MidnightIntegration\MidnightIntegration.exe" %*
