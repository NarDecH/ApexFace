@echo off
setlocal
title ApexFace Build and Run
cd /d "%~dp0"

rem ============================================================
rem  build-and-run.bat - build the project, then launch the GUI
rem  (handy while tweaking code; for a fresh configure: build.bat clean)
rem
rem  สร้างโปรแกรมใหม่แล้วเปิดทันที (ถ้า build ไม่ผ่านจะหยุดให้ดู error)
rem ============================================================

call build.bat
if errorlevel 1 (
    echo [ApexFace] build failed - see the errors above.
    pause
    exit /b 1
)
call "%~dp0ApexFace.bat"
