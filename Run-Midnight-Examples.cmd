@echo off
setlocal
cd /d "%~dp0"
if not exist "Midnight\runtime" mkdir "Midnight\runtime"
if not exist "Midnight\runtime\Integration_Evaluation_4c244d7.noisette" copy "Midnight\examples\Midnight_Chataigne_Integration.noisette" "Midnight\runtime\Integration_Evaluation_4c244d7.noisette" >nul
call "%~dp0Run-Midnight-Integration.cmd" "%~dp0Midnight\runtime\Integration_Evaluation_4c244d7.noisette"
