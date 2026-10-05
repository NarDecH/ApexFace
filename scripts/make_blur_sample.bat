@echo off
rem One-off: compile tools\make_blur.cpp and create the synthetic blurred sample
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
call C:\_OpenCV\opencv-env.bat >nul
cd /d "%~dp0.."
cl /nologo /EHsc /O2 /utf-8 /I C:\_OpenCV\install\include tools\make_blur.cpp ^
   /link /LIBPATH:C:\_OpenCV\install\x64\vc17\lib opencv_core4140.lib opencv_imgproc4140.lib opencv_imgcodecs4140.lib
if errorlevel 1 exit /b 1
make_blur.exe samples\apollo11_crew.jpg samples\apollo11_crew_blurred.jpg 7
del make_blur.obj make_blur.exe 2>nul
