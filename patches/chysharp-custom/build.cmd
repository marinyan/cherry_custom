@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File embed-payload.ps1
if errorlevel 1 exit /b 1
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
cl /nologo /c /W4 /WX /wd4100 /O2 /MT /utf-8 /std:c17 /DUNICODE /D_UNICODE /I. /Ibuild src\chysharp.c /Fobuild\chysharp.obj
if errorlevel 1 exit /b 1
cl /nologo /c /W4 /WX /O2 /MT /Ibuild wheel-image.c /Fobuild\wheel-image.obj
if errorlevel 1 exit /b 1
cl /nologo /c /W4 /WX /O2 /MT keys-image.c /Fobuild\keys-image.obj
if errorlevel 1 exit /b 1
pushd src
rc /nologo /fo ..\build\chysharp.res chysharp.rc
set "RC_RESULT=%ERRORLEVEL%"
popd
if not "%RC_RESULT%"=="0" exit /b 1
link /nologo /out:build\chysharp-custom.exe /subsystem:windows /machine:x86 /manifest:no build\chysharp.obj build\wheel-image.obj build\keys-image.obj build\chysharp.res user32.lib gdi32.lib comdlg32.lib advapi32.lib
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /MT /utf-8 /DUNICODE /D_UNICODE /Isrc custom-test.c build\chysharp.obj build\wheel-image.obj build\keys-image.obj /Fobuild\custom-test.obj /Febuild\custom-test.exe /link /subsystem:console user32.lib gdi32.lib comdlg32.lib advapi32.lib
if errorlevel 1 exit /b 1
copy /y build\chysharp-custom.exe chysharp-custom.exe >nul
if errorlevel 1 exit /b 1
echo Built: %CD%\chysharp-custom.exe
