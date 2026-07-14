@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "GPU_CATALOG=%ROOT_DIR%\data\local\passmark_gpu_catalog.csv"
set "OUTPUT_EXE=%BUILD_DIR%\bin\hardware_requirements_embedded.exe"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    exit /b 1
)

if not exist "%GPU_CATALOG%" (
    echo Error: local GPU catalog was not found:
    echo   %GPU_CATALOG%
    echo Run tools\update_passmark_gpu_catalog.py first.
    exit /b 1
)

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" ^
    -DHWINFO_BUILD_LOCAL_REQUIREMENTS=ON ^
    "-DHWINFO_LOCAL_GPU_CATALOG=%GPU_CATALOG%" ^
    -DHWINFO_LOCAL_MIN_CPU_CORES=4 ^
    "-DHWINFO_LOCAL_MIN_GPU_MODEL=GeForce RTX 3060" ^
    -DHWINFO_LOCAL_MIN_MEMORY_GIB=16 ^
    -DHWINFO_LOCAL_REQUIRE_SSD=ON
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release --target hardware_requirements_embedded --parallel
if errorlevel 1 exit /b 1

if not exist "%OUTPUT_EXE%" (
    echo Error: build completed but the expected executable was not found:
    echo   %OUTPUT_EXE%
    exit /b 1
)

echo.
echo Build complete:
echo   %OUTPUT_EXE%

endlocal
