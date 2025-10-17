@echo off
setlocal enabledelayedexpansion

set BUILD_DIR=%~dp0build\vs
if not exist "%BUILD_DIR%" (
    echo [!] Build directory not found. Run build.bat first to configure CMake.
    exit /b 1
)

echo [*] Building TSH.Driver (Release|x64) via CMake...
cmake --build "%BUILD_DIR%" --target TSH.Driver --config Release
if errorlevel 1 goto :error

echo [*] Driver build complete. Artifacts under %BUILD_DIR%\driver.
exit /b 0

:error
echo [!] Driver build failed.
exit /b 1
