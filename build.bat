@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    exit /b 1
)

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b %errorlevel%

echo.
echo Build complete:
echo   %BUILD_DIR%\bin\system_info.exe
echo   %BUILD_DIR%\bin\hardware_summary.exe

endlocal
