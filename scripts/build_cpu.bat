@echo off
rem Build ApexFace against the lean CPU-only OpenCV (for the portable release)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
cmake -S . -B build-cpu -A x64 -DOpenCV_DIR="C:/_OpenCV/projects/ApexFace/_tmp/ocv-cpu-install" || exit /b 1
cmake --build build-cpu --config Release -- /m /v:m || exit /b 1
echo [apexface-cpu] DONE: build-cpu\Release\ApexFace.exe
