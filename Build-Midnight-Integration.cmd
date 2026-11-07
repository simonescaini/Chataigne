@echo off
setlocal
cd /d "%~dp0"
set "INTEGRATION_PYTHON="
if exist "Midnight\python-path.txt" set /p INTEGRATION_PYTHON=<"Midnight\python-path.txt"
if not defined INTEGRATION_PYTHON exit /b 2
"%INTEGRATION_PYTHON%" "Midnight\build_integration.py" %*
exit /b %errorlevel%
