@echo off
setlocal
title ApexFace CLI
cd /d "%~dp0"

rem ============================================================
rem  run-cli.bat - analyze a photo folder from the command line
rem
rem  how to use: drag a photo folder onto this file and press
rem              Enter when prompted, or run:
rem                run-cli.bat <folder> [extra options]
rem              e.g.  run-cli.bat D:\photos --no-annotated --min-face 32
rem
rem  results go to <folder>\_apexface_report (unless you pass --out)
rem  and the script offers to open the HTML report when finished.
rem
rem  วิธีใช้: ลากโฟลเดอร์รูปมาวางบนไฟล์นี้ แล้วกด Enter
rem  ผลลัพธ์อยู่ที่ <โฟลเดอร์>\_apexface_report และจะถามเปิดรายงานให้
rem ============================================================

set "EXE="
if exist "build\Release\apexface-cli.exe" (
    if exist "C:\_OpenCV\opencv-env.bat" call "C:\_OpenCV\opencv-env.bat" >nul 2>&1
    set "EXE=build\Release\apexface-cli.exe"
) else if exist "build-cpu\Release\apexface-cli.exe" (
    set "EXE=build-cpu\Release\apexface-cli.exe"
) else if exist "_release\ApexFace-1.0.0-win64\apexface-cli.exe" (
    set "EXE=_release\ApexFace-1.0.0-win64\apexface-cli.exe"
)
if not defined EXE (
    echo [ApexFace] apexface-cli.exe not found. Run build.bat first.
    pause
    exit /b 1
)

set "FOLDER=%~1"
if "%~1"=="" (
    echo Drag a photo folder onto this window/file, or type a path below.
    set /p "FOLDER=Photo folder: "
)
set "FOLDER=%FOLDER:"=%"
if "%FOLDER%"=="" (
    echo [ApexFace] No folder given.
    pause
    exit /b 1
)
if not exist "%FOLDER%\" (
    echo [ApexFace] Folder not found: %FOLDER%
    pause
    exit /b 1
)

rem pass any extra options (2nd argument onwards) through to the CLI
set "REST="
:parse
if not "%~2"=="" (
    call set "REST=%%REST%% %~2"
    shift
    goto parse
)

echo [ApexFace] analyzing folder: %FOLDER%
echo ------------------------------------------------------------
"%EXE%" "%FOLDER%"%REST%
set "RC=%ERRORLEVEL%"
echo ------------------------------------------------------------
if not "%RC%"=="0" goto failed
if not exist "%FOLDER%\_apexface_report\index.html" goto done
echo [ApexFace] report: %FOLDER%\_apexface_report\index.html
set /p "OPEN=Open the report now? [Y/n]: "
if /i "%OPEN%"=="n" goto done
start "" "%FOLDER%\_apexface_report\index.html"
goto done

:failed
echo [ApexFace] finished with exit code %RC% (see the log inside the report folder)

:done
pause
exit /b %RC%
