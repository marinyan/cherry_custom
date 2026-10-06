@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /W4 /WX /O1 /GS- /Zl /c wheel.c /Fobuild\wheel.obj
if errorlevel 1 exit /b 1
link /nologo build\wheel.obj /out:wheel-hook.bin /entry:WheelMain /subsystem:windows /machine:x86 /nodefaultlib /base:0x400000 /fixed:no /dynamicbase:no /incremental:no /safeseh:no
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX wheel-test.c /Fobuild\wheel-test.obj /Febuild\wheel-test.exe
if errorlevel 1 exit /b 1
build\wheel-test.exe
exit /b %errorlevel%
