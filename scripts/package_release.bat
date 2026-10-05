@echo off
rem Package the portable Windows x64 release zip
setlocal enabledelayedexpansion
cd /d "%~dp0.."
set "VER=1.0.2"
set "STG=_release\ApexFace-%VER%-win64"
if not exist "build-cpu\Release\ApexFace.exe" (
    echo [package] build-cpu missing - run scripts\build_cpu.bat first
    exit /b 1
)
rmdir /s /q "%STG%" 2>nul
mkdir "%STG%"

copy /y "build-cpu\Release\ApexFace.exe"      "%STG%\" >nul
copy /y "build-cpu\Release\apexface-cli.exe"  "%STG%\" >nul

rem lean CPU-only OpenCV runtime
for %%D in (core imgproc imgcodecs objdetect dnn calib3d features2d flann) do (
    copy /y "_tmp\ocv-cpu-install\x64\vc17\bin\opencv_%%D4140.dll" "%STG%\" >nul
)
rem MSVC runtime (ships so the app runs without the VC++ redist installed)
for %%D in (msvcp140.dll vcruntime140.dll vcruntime140_1.dll concrt140.dll) do (
    copy /y "C:\Windows\System32\%%D" "%STG%\" >nul
)

xcopy /e /i /y "models"  "%STG%\models"  >nul
xcopy /e /i /y "fonts"   "%STG%\fonts"   >nul
xcopy /e /i /y "samples" "%STG%\samples" >nul
mkdir "%STG%\docs"
copy /y "docs\*.html" "%STG%\docs\" >nul
copy /y "docs\*.md"   "%STG%\docs\" >nul
copy /y "README.md"   "%STG%\" >nul
copy /y "LICENSE"     "%STG%\" >nul

powershell -NoProfile -Command "Compress-Archive -Path '%STG%' -DestinationPath '_release\ApexFace-%VER%-win64.zip' -Force"
echo [package] done: _release\ApexFace-%VER%-win64.zip
dir "_release"
