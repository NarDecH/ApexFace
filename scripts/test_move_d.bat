@echo off
rem Test 2: build a mixed test folder (sharp / grade-D / no-face) and run --move-d
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
call C:\_OpenCV\opencv-env.bat >nul
cd /d "%~dp0.."

if not exist make_blur.exe (
    cl /nologo /EHsc /O2 /utf-8 /I C:\_OpenCV\install\include tools\make_blur.cpp ^
       /link /LIBPATH:C:\_OpenCV\install\x64\vc17\lib opencv_core4140.lib opencv_imgproc4140.lib opencv_imgcodecs4140.lib
    if errorlevel 1 exit /b 1
    del make_blur.obj 2>nul
)

if not exist _tmp\movetest mkdir _tmp\movetest
copy /y samples\obama_portrait.jpg _tmp\movetest\ >nul
copy /y samples\apollo11_crew.jpg _tmp\movetest\ >nul
make_blur.exe samples\apollo11_crew.jpg _tmp\movetest\blurry_D.jpg 7
make_blur.exe samples\apollo11_crew.jpg _tmp\movetest\very_blurry_noface.jpg 30

build\Release\apexface-cli.exe _tmp\movetest --out _tmp\movetest_report --move-d --quiet
echo --- source folder after move ---
dir /b _tmp\movetest
echo --- rejects folder ---
dir /s /b _tmp\movetest\_apexface_rejects\*.jpg 2>nul
echo --- csv rows ---
type _tmp\movetest_report\data.csv
