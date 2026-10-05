@echo off
rem Run the ApexFace CLI with the OpenCV DLLs on PATH
rem usage: run_cli.bat <folder> [options]
call C:\_OpenCV\opencv-env.bat
"%~dp0..\build\Release\apexface-cli.exe" %*
