@echo off
rem ============================================================
rem  ApexFace build script (MSVC x64 + OpenCV from C:\_OpenCV)
rem  usage: build.bat            configure + build Release
rem         build.bat clean      reconfigure from scratch
rem ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"
if not defined VS (
    echo [apexface] Visual Studio not found via vswhere
    exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

if "%~1"=="clean" (
    echo [apexface] removing build directory
    rmdir /s /q build 2>nul
)

cmake -S . -B build -A x64 %2 %3 %4
if errorlevel 1 exit /b 1
cmake --build build --config Release -- /m /v:m
if errorlevel 1 exit /b 1

echo.
echo [apexface] build OK:
echo   build\Release\ApexFace.exe
echo   build\Release\apexface-cli.exe
echo Run with: scripts\run_gui.bat  or  scripts\run_cli.bat ^<folder^>
