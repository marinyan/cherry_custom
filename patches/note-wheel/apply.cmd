@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0apply.ps1"
if errorlevel 1 (
  echo Patch failed. See the message above.
  pause
  exit /b 1
)
echo Start the executable shown above to use wheel scrolling.
pause
