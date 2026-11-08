@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0windows\setup.ps1" %*
set "result=%ERRORLEVEL%"
if "%~1"=="" pause
exit /b %result%
