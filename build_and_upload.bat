@echo off
REM Quick build and upload script for TokenDisplay

echo ========================================
echo  ESP32-C6 Token Display Builder
echo ========================================
echo.

echo [1/3] Cleaning build...
pio run --target clean

echo.
echo [2/3] Building project...
pio run

echo.
echo [3/3] Uploading to ESP32-C6...
pio run --target upload

echo.
echo ========================================
echo  Build and Upload Complete!
echo ========================================
echo.
echo Opening Serial Monitor...
echo Press Ctrl+C to exit
echo.

pio device monitor
