@echo off
rem Run the ApexFace GUI with the OpenCV DLLs on PATH
call C:\_OpenCV\opencv-env.bat
start "" "%~dp0..\build\Release\ApexFace.exe" %*
