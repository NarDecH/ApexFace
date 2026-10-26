@echo off
rem Regenerate assets\icon\apexface.ico + apexface_256.png with OpenCV
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
cl /nologo /EHsc /O2 /utf-8 /std:c++20 /I C:\_OpenCV\install\include tools\make_icon.cpp ^
   /link /LIBPATH:C:\_OpenCV\install\x64\vc17\lib opencv_core4140.lib opencv_imgproc4140.lib opencv_imgcodecs4140.lib
if errorlevel 1 exit /b 1
del make_icon.obj 2>nul
call C:\_OpenCV\opencv-env.bat >nul
if "%~1"=="" (make_icon.exe assets\icon) else (make_icon.exe %1)
