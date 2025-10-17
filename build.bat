@echo off
setlocal enabledelayedexpansion

set BUILD_DIR=%~dp0build\vs
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

echo [*] Configuring CMake project...
cmake -S "%~dp0." -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :error

echo [*] Building user-mode and driver targets (Debug)...
cmake --build "%BUILD_DIR%" --config Debug
if errorlevel 1 goto :error

echo [*] Build completed. Binaries are under %BUILD_DIR%.
exit /b 0

:error
echo [!] Build failed. Review the log above.
exit /b 1
