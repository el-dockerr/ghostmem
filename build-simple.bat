@echo off
REM Quick build script for Windows without CMake (direct MSVC compilation)

echo Building GhostMem (Direct Compilation)...

REM Check for MSVC
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MSVC compiler not found!
    echo Please run this from "Developer Command Prompt for VS" or "x64 Native Tools Command Prompt"
    exit /b 1
)

REM Compile
cl /EHsc /std:c++17 /O2 ^
    src/main.cpp ^
    src/ghostmem/GhostMemoryManager.cpp ^
    src/3rdparty/lz4.c ^
    /I src ^
    /Fe:ghostmem_demo.exe

if %ERRORLEVEL% EQU 0 (
    echo Build complete! Run with: ghostmem_demo.exe
    REM Clean up intermediate files
    del *.obj 2>nul
) else (
    echo Build failed!
    exit /b 1
)
