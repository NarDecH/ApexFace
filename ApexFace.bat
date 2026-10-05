@echo off
setlocal
title ApexFace Launcher
cd /d "%~dp0"

rem ============================================================
rem  ApexFace.bat - launch the ApexFace GUI
rem
rem  how to use: double-click, or drag a photo folder onto this
rem              file, or run:  ApexFace.bat [photo-folder]
rem
rem  picks the best build available on this machine, in order:
rem    1) build\Release\ApexFace.exe          (full CUDA dev build)
rem    2) build-cpu\Release\ApexFace.exe      (portable CPU build)
rem    3) _release\ApexFace-*-win64\...       (packaged release)
rem
rem  วิธีใช้: ดับเบิลคลิก หรือ ลากโฟลเดอร์รูปมาวางบนไฟล์นี้
rem ============================================================

set "EXE="
if exist "build\Release\ApexFace.exe" (
    rem the CUDA build needs CUDA/cuDNN DLLs on PATH
    if exist "C:\_OpenCV\opencv-env.bat" call "C:\_OpenCV\opencv-env.bat" >nul 2>&1
    set "EXE=build\Release\ApexFace.exe"
) else if exist "build-cpu\Release\ApexFace.exe" (
    set "EXE=build-cpu\Release\ApexFace.exe"
) else (
    for /d %%D in ("_release\ApexFace-*-win64") do if exist "%%D\ApexFace.exe" set "EXE=%%D\ApexFace.exe"
)

if not defined EXE (
    echo [ApexFace] ApexFace.exe not found.
    echo            Run build.bat first, or extract the release zip under _release\
    pause
    exit /b 1
)

echo [ApexFace] starting: %EXE%
if "%~1"=="" (
    start "" "%EXE%"
) else (
    start "" "%EXE%" "%~f1"
)
exit /b 0
