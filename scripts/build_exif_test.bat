@echo off
rem Compile tools\exif_test.cpp against the project's ImageIO and run a
rem ground-truth comparison against scripts\dump_orientation.ps1 output.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
cl /nologo /EHsc /O2 /utf-8 /std:c++20 /I src\core /I C:\_OpenCV\install\include ^
   tools\exif_test.cpp src\core\ImageIO.cpp ^
   /link /LIBPATH:C:\_OpenCV\install\x64\vc17\lib opencv_core4140.lib opencv_imgproc4140.lib opencv_imgcodecs4140.lib
if errorlevel 1 exit /b 1
del exif_test.obj 2>nul
