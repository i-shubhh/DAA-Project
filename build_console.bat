@echo off
setlocal
echo ===============================================================================
echo   Compiling Optimal Delivery Route Planning - Console Application (C Language)
echo ===============================================================================

where gcc >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC compiler was not found in PATH!
    echo Please install MinGW or MSYS2 and ensure gcc.exe is in your system PATH.
    pause
    exit /b 1
)

if not exist bin mkdir bin

gcc -std=c99 -Wall -Wextra -O2 -ICommon Common/route.c Common/nearest_neighbor.c Common/branch_bound.c Console/main.c -o bin/delivery_planner_console.exe

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Compiled successfully to bin\delivery_planner_console.exe
echo ===============================================================================
echo Launching Console Application...
echo ===============================================================================
bin\delivery_planner_console.exe
pause

