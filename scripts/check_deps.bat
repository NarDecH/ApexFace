@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
for %%D in (core imgproc imgcodecs objdetect dnn) do (
    echo === opencv_%%D4140.dll ===
    dumpbin /dependents C:\_OpenCV\install\x64\vc17\bin\opencv_%%D4140.dll | findstr /i ".dll"
)
echo === ApexFace.exe ===
dumpbin /dependents "C:\_OpenCV\projects\ApexFace\build\Release\ApexFace.exe" | findstr /i ".dll"
