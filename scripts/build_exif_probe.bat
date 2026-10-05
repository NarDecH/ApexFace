@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
cl /nologo /EHsc /O2 /utf-8 /std:c++20 /I src\core /I C:\_OpenCV\install\include ^
   tools\exif_probe.cpp src\core\ImageIO.cpp ^
   /link /LIBPATH:C:\_OpenCV\install\x64\vc17\lib opencv_core4140.lib opencv_imgproc4140.lib opencv_imgcodecs4140.lib
if errorlevel 1 exit /b 1
del exif_probe.obj 2>nul
call C:\_OpenCV\opencv-env.bat >nul
exif_probe.exe _tmp\portrait_test\DEC05886.JPG _tmp\portrait_test\DEC05887.JPG
