@echo off
setlocal

set PROJECT=%~dp0driver\MemoryPatcherDrv.vcxproj
if not exist "%PROJECT%" (
    echo Driver project not found at %PROJECT%
    exit /b 1
)

echo [*] Building MemoryPatcher kernel driver (Release|x64)...
msbuild "%PROJECT%" /p:Configuration=Release /p:Platform=x64 || goto :error

echo [*] Build complete. Output located under driver\build\driver\
exit /b 0

:error
echo [!] Driver build failed.
exit /b 1
