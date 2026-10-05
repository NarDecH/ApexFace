@echo off
rem Build a lean CPU-only OpenCV for the portable release
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "SRC=C:\_OpenCV\opencv"
set "BLD=C:\_OpenCV\projects\ApexFace\_tmp\ocv-cpu-build"
set "INS=C:\_OpenCV\projects\ApexFace\_tmp\ocv-cpu-install"

if not exist "%BLD%\CMakeCache.txt" (
    cmake -S "%SRC%" -B "%BLD%" -G "Visual Studio 17 2022" -A x64 ^
        -DCMAKE_INSTALL_PREFIX="%INS%" ^
        -DBUILD_LIST=core,imgproc,imgcodecs,objdetect,dnn,calib3d,features2d,flann ^
        -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF ^
        -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF ^
        -DBUILD_opencv_python3=OFF -DWITH_CUDA=OFF -DWITH_FFMPEG=OFF ^
        -DWITH_TBB=OFF -DCMAKE_CXX_FLAGS="/utf-8" || exit /b 1
)
cmake --build "%BLD%" --config Release -- /m /v:m || exit /b 1
cmake --install "%BLD%" --config Release || exit /b 1
echo [ocv-cpu] DONE
dir /b "%INS%\x64\vc17\bin\*.dll"
