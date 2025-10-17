@echo off
setlocal

python -m pip install --upgrade pip >nul
python -m pip install -e .[dev]
if errorlevel 1 goto :error
python -m pytest
if errorlevel 1 goto :error

echo [*] Build successful. Artifacts remain in source tree.
exit /b 0

:error
echo [!] Build failed. Review the log above.
exit /b 1
