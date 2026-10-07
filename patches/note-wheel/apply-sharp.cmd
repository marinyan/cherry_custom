@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0apply.ps1" -Variant ChySharp
if errorlevel 1 (
  echo Patch failed. See the message above.
  pause
  exit /b 1
)
echo Start cherry_1\cherry-sharp-wheel.exe to use wheel scrolling.
pause
