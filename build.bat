@echo off
setlocal
echo ===============================================================================
echo   OPTIMAL DELIVERY ROUTE PLANNING FOR E-COMMERCE APPLICATIONS - DAA PROJECT
echo   Build Script: Windows MinGW / MSYS2 / GCC
echo ===============================================================================

where gcc >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC compiler was not found in system PATH!
    echo Please install MinGW-w64 or MSYS2 and add gcc to your PATH.
    exit /b 1
)

if not exist bin mkdir bin
if not exist database mkdir database

echo.
echo [1/4] Compiling SQLite Engine Object: backend\sqlite3.o ...
if not exist backend\sqlite3.o (
    gcc -c -O2 backend\sqlite3.c -o backend\sqlite3.o -DSQLITE_THREADSAFE=1
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Failed to compile sqlite3.c!
        exit /b %ERRORLEVEL%
    )
) else (
    echo       backend\sqlite3.o is up to date.
)

echo.
echo [2/4] Compiling Web Application C Backend Server: bin\delivery_server.exe ...
gcc -std=c99 -Wall -Wextra -O2 -ICommon -Ibackend -DSQLITE_THREADSAFE=1 backend\server.c backend\database.c backend\geocoding.c backend\routing.c backend\http_client.c backend\cJSON.c backend\sqlite3.o Common\route.c Common\nearest_neighbor.c Common\branch_bound.c -o bin\delivery_server.exe -lws2_32

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile Web Backend Server!
    exit /b %ERRORLEVEL%
)

echo.
echo [3/4] Compiling Console Application: bin\delivery_planner_console.exe ...
gcc -std=c99 -Wall -Wextra -O2 -ICommon Common\route.c Common\nearest_neighbor.c Common\branch_bound.c Console\main.c -o bin\delivery_planner_console.exe

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile Console Application!
    exit /b %ERRORLEVEL%
)

echo.
echo [4/4] Compiling Verification Suite: bin\verify_all.exe ...
gcc -std=c99 -Wall -Wextra -O2 -ICommon Common\route.c Common\nearest_neighbor.c Common\branch_bound.c tests\verify_all.c -o bin\verify_all.exe

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile Test Suite!
    exit /b %ERRORLEVEL%
)

echo.
echo ===============================================================================
echo [SUCCESS] All components compiled successfully!
echo.
echo   * Web Backend Server  : bin\delivery_server.exe
echo   * Console Application : bin\delivery_planner_console.exe
echo   * Verification Suite  : bin\verify_all.exe
echo.
echo To start the Web Application:
echo   1. Run: bin\delivery_server.exe
echo   2. Open http://localhost:8080 in your web browser.
echo.
echo To run the Console Application:
echo   Run: bin\delivery_planner_console.exe
echo ===============================================================================
