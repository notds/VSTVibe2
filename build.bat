@echo off
REM VSTVibe2 Build Script
REM This script configures and builds the VSTVibe2 VST3 plugin

setlocal enabledelayedexpansion

REM Set VST3 SDK path
set VST3_SDK_PATH=C:\code\VST_SDK\vst3sdk

REM Check if VST3 SDK exists
if not exist "!VST3_SDK_PATH!\public.sdk" (
    echo ERROR: VST3 SDK not found at !VST3_SDK_PATH!
    echo Please set the correct VST3_SDK_PATH in this script.
    exit /b 1
)

echo ======================================
echo VSTVibe2 Build Script
echo ======================================
echo VST3 SDK: !VST3_SDK_PATH!

REM Create build directory if it doesn't exist
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

REM Navigate to build directory
cd /d "%~dp0build"

REM Configure with CMake
echo.
echo Configuring CMake...
cmake -DVST3_SDK_PATH="!VST3_SDK_PATH!" -G "Visual Studio 17 2022" ..
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build in Release mode
echo.
echo Building Release configuration...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo VSTVibe2 build completed successfully!
echo ======================================
exit /b 0
echo Build completed successfully!
echo ======================================
echo.
echo Plugin location:
echo %~dp0build\Release\VSTVibe.vst3
echo.

REM Check if plugin was created and show file size
if exist "Release\VSTVibe.vst3" (
    for %%A in ("Release\VSTVibe.vst3") do (
        set size=%%~zA
        set /a size_kb=!size!/1024
        echo File size: !size_kb! KB
    )
) else (
    echo WARNING: Plugin file not found!
    exit /b 1
)

exit /b 0
