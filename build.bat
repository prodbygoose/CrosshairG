@echo off
echo ============================================
echo  CrosshairG v1.0 - Are you reading this, you shouldnt be..
echo ============================================

set "SCRIPT_DIR=%~dp0"
set "GXX="
set "WINDRES="

if exist "%SCRIPT_DIR%mingw64\bin\g++.exe" (
    set "GXX=%SCRIPT_DIR%mingw64\bin\g++.exe"
    set "WINDRES=%SCRIPT_DIR%mingw64\bin\windres.exe"
)

if not defined GXX (
    for /r "%SCRIPT_DIR%" %%F in (g++.exe) do (
        if not defined GXX set "GXX=%%~F"
    )
)
if not defined WINDRES (
    for /r "%SCRIPT_DIR%" %%F in (windres.exe) do (
        if not defined WINDRES set "WINDRES=%%~F"
    )
)

if not defined GXX (
    where g++ >nul 2>nul
    if %errorlevel% equ 0 set "GXX=g++"
)
if not defined WINDRES (
    where windres >nul 2>nul
    if %errorlevel% equ 0 set "WINDRES=windres"
)

if not defined GXX (
    echo ERROR: Could not find g++.exe
    pause
    exit /b 1
)

echo Found g++ at: %GXX%
echo.

if not exist "%SCRIPT_DIR%build" mkdir "%SCRIPT_DIR%build"

:: Compile icon resource if icon.ico exists
set "RES_OBJ="
if exist "%SCRIPT_DIR%src\icon.ico" (
    if defined WINDRES (
        echo Compiling icon resource...
        "%WINDRES%" "%SCRIPT_DIR%src\resource.rc" -O coff -o "%SCRIPT_DIR%build\resource.o" --include-dir "%SCRIPT_DIR%src"
        if %errorlevel% equ 0 (
            set "RES_OBJ=%SCRIPT_DIR%build\resource.o"
            echo Icon compiled successfully.
        ) else (
            echo Warning: Icon compilation failed, building without icon.
        )
    )
) else (
    echo Note: No src\icon.ico found - building without custom icon.
)

echo.
echo Building CrosshairG_v1.0.exe ...
echo.

"%GXX%" -O2 ^
    -o "%SCRIPT_DIR%build\CrosshairG_v1.0.exe" ^
    "%SCRIPT_DIR%src\crosshair.cpp" ^
    %RES_OBJ% ^
    -lgdi32 -luser32 -lshell32 -lcomdlg32 -lcomctl32 -ldwmapi ^
    -mwindows -municode -std=c++17 ^
    -static -static-libgcc -static-libstdc++

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED. See errors above.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  BUILD SUCCESS!
echo  Output: build\CrosshairG_v1.0.exe
echo  Ctrl+F5 = Toggle menu
echo  Ctrl+F6 = Toggle crosshair
echo ============================================
echo.

set /p LAUNCH=Launch now? (y/n): 
if /i "%LAUNCH%"=="y" start "" "%SCRIPT_DIR%build\CrosshairG_v1.0.exe"
pause