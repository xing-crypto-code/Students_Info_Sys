@echo off
setlocal
cd /d "%~dp0"

if not exist "%~dp0bin\main.exe" (
    echo [ERROR] bin\main.exe not found. Please run build.bat first.
    pause
    exit /b 1
)

rem The program looks up fonts / icons / help images through the LVGL "A:" drive,
rem which is mapped to the current working directory. So bin\ must be the CWD.
cd /d "%~dp0bin"

echo Starting main.exe ...
start "" "%~dp0bin\main.exe"
exit /b 0
