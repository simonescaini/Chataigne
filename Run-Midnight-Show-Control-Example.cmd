@echo off
setlocal
cd /d "%~dp0"
call Run-Midnight-Show-Control.cmd "%~dp0Midnight\examples\Show_Control_Integrated.noisette"
