@echo off
REM Build script for GhostMem on Windows

echo ================================================
echo   GhostMem Windows Build Script
echo ================================================
echo.

REM Check if CMake is available
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found! Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    exit /b 1
)

REM Create build directory
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

echo.
echo [1/3] Configuring project with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo [2/3] Building project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    cd ..
    exit /b 1
)

echo.
echo [3/3] Build complete!
echo.
echo Output files:
echo   - Static library: build\Release\ghostmem.lib
echo   - Shared library: build\Release\ghostmem.dll
echo   - Demo program:   build\Release\ghostmem_demo.exe
echo.
echo To run the demo:
echo   cd build\Release
echo   ghostmem_demo.exe
echo.

cd ..
echo ================================================
echo   Build completed successfully! 
echo ================================================
