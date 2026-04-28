@echo off
REM VSTVibe2 Clean & Rebuild Script
REM This script removes the build directory and performs a clean build

echo ======================================
echo VSTVibe2 Clean & Rebuild
echo ======================================
echo.

REM Check if build directory exists and remove it
if exist "build" (
    echo Removing build directory...
    rmdir /s /q build
    if %errorlevel% neq 0 (
        echo ERROR: Failed to remove build directory!
        exit /b 1
    )
)

REM Run normal build
echo.
call "%~dp0build.bat"
exit /b %errorlevel%
