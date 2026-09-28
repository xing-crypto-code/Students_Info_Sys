@echo off
setlocal
cd /d "%~dp0"

set "ROOT=%~dp0"
set "PATH=%ROOT%mingw64\bin;%ROOT%cmake-4.1.2-windows-x86_64\bin;%ROOT%;%PATH%"

echo ==============================================
echo  Students Info Sys - Build
echo ==============================================
echo.

if not exist "%ROOT%build\build.ninja" (
    echo [1/2] Configuring CMake ...
    "%ROOT%cmake-4.1.2-windows-x86_64\bin\cmake.exe" -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%ROOT%ninja.exe"
    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configure failed.
        pause
        exit /b 1
    )
) else (
    echo [1/2] CMake already configured, skipping.
)

echo.
echo [2/2] Compiling ...
"%ROOT%ninja.exe" -C "%ROOT%build" main
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo ==============================================
echo  Build succeeded: bin\main.exe
echo  Run the app with: run.bat
echo ==============================================
pause
exit /b 0
