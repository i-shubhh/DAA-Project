@echo off
setlocal
echo ===============================================================================
echo   Starting Optimal Delivery Route Planning Web Application Server...
echo ===============================================================================

if not exist bin\delivery_server.exe (
    echo [INFO] Server binary not found. Building project first...
    call build.bat
)

echo.
echo [SERVER] Launching C HTTP Backend on http://localhost:8080 ...
echo [SERVER] Press Ctrl+C in this console to stop the server.
echo.
start http://localhost:8080
bin\delivery_server.exe
pause

