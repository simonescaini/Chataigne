@echo off
setlocal
cd /d "%~dp0"
set "POC_PYTHON="
if exist "Midnight\python-path.txt" set /p POC_PYTHON=<"Midnight\python-path.txt"
if defined POC_PYTHON if exist "%POC_PYTHON%" goto run
where py >nul 2>nul
if not errorlevel 1 (py -3 Midnight\build.py %* & exit /b)
echo Python 3.11 or later is required. Set Midnight\python-path.txt to its python.exe path.
exit /b 2
:run
"%POC_PYTHON%" "Midnight\build.py" %*
exit /b %errorlevel%
