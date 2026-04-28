@echo off
REM VSTVibe2 Clean Script
REM This script removes all build artifacts

setlocal

echo ======================================
echo VSTVibe2 Clean
echo ======================================
echo.

REM Check if build directory exists
if exist "build" (
    echo Removing build directory...
    rmdir /s /q build
    if %errorlevel% neq 0 (
        echo ERROR: Failed to remove build directory!
        exit /b 1
    )
    echo Build directory removed.
) else (
    echo Build directory not found - already clean.
)

echo.
echo Clean complete!
exit /b 0
