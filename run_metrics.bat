@echo off
REM Run GhostMem performance metrics tests and save results

echo ============================================
echo GhostMem Performance Metrics Runner
echo ============================================
echo.

REM Check if build directory exists
if not exist "build\Release\ghostmem_tests.exe" (
    echo ERROR: Tests not built. Please run build.bat first.
    exit /b 1
)

REM Create results directory
if not exist "metrics_results" mkdir metrics_results

REM Generate timestamp for filename
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set timestamp=%datetime:~0,8%_%datetime:~8,6%

REM Set output file
set OUTPUT_FILE=metrics_results\metrics_%timestamp%.txt

echo Running tests...
echo Results will be saved to: %OUTPUT_FILE%
echo.

REM Run tests and save output
build\Release\ghostmem_tests.exe > "%OUTPUT_FILE%" 2>&1

REM Check if tests passed
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo SUCCESS: All tests passed!
    echo ============================================
    echo.
    echo Key Metrics Summary:
    echo -------------------
    findstr /C:"Compression:" /C:"Slowdown:" /C:"savings:" /C:"Physical RAM limit:" "%OUTPUT_FILE%"
    echo.
    echo Full results saved to: %OUTPUT_FILE%
) else (
    echo.
    echo ============================================
    echo ERROR: Some tests failed!
    echo ============================================
    echo Check %OUTPUT_FILE% for details
    exit /b 1
)

echo.
echo To compare with previous results:
echo   fc metrics_results\metrics_PREVIOUS.txt "%OUTPUT_FILE%"
echo.

exit /b 0
