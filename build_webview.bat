@echo off
echo ============================================
echo  CrosshairG v1.3
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
if not defined GXX ( echo ERROR: g++ not found & pause & exit /b 1 )

set "WV2_INC="
set "WV2_LIB="
set "WV2_DLL="

for /d %%D in ("%SCRIPT_DIR%deps\*") do (
    if exist "%%D\build\native\include\WebView2.h" (
        if not defined WV2_INC (
            set "WV2_INC=%%D\build\native\include"
            set "WV2_LIB=%%D\build\native\x64"
            set "WV2_DLL=%%D\build\native\x64\WebView2Loader.dll"
        )
    )
)

if not defined WV2_INC (
    echo ERROR: WebView2.h not found.
    echo Expected: deps\^<packagename^>\build\native\include\WebView2.h
    pause & exit /b 1
)

echo Found g++    : %GXX%
echo Found WV2    : %WV2_INC%
echo.

echo [1/3] Building React UI...
cd "%SCRIPT_DIR%ui"
if not exist "node_modules" call npm install
call npm run build
if %errorlevel% neq 0 ( cd "%SCRIPT_DIR%" & echo React build failed & pause & exit /b 1 )
cd "%SCRIPT_DIR%"
echo React UI built successfully.
echo.

echo [2/3] Compiling resources...
if not exist "%SCRIPT_DIR%build" mkdir "%SCRIPT_DIR%build"
set "RES_OBJ="
if exist "%SCRIPT_DIR%src\icon.ico" (
    if defined WINDRES (
        "%WINDRES%" "%SCRIPT_DIR%src\resource.rc" -O coff -o "%SCRIPT_DIR%build\resource.o" --include-dir "%SCRIPT_DIR%src"
        if %errorlevel% equ 0 set "RES_OBJ=%SCRIPT_DIR%build\resource.o"
    )
)
echo Icon compiled.

echo [3/3] Compiling CrosshairG...
echo.

"%GXX%" -O2 ^
    -o "%SCRIPT_DIR%build\CrosshairG_v1.3.exe" ^
    "%SCRIPT_DIR%src\crosshair_webview.cpp" ^
    %RES_OBJ% ^
    -I"%WV2_INC%" ^
    -L"%WV2_LIB%" ^
    -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -ldwmapi -lshlwapi ^
    -lWebView2Loader ^
    -mwindows -municode -std=c++17 ^
    -static-libgcc -static-libstdc++

if %errorlevel% neq 0 ( echo BUILD FAILED & pause & exit /b 1 )

echo Copying runtime...
copy /Y "%WV2_DLL%" "%SCRIPT_DIR%build\" >nul

for %%F in ("%GXX%") do set "MINGW_BIN=%%~dpF"
set "MINGW_BIN=%MINGW_BIN:~0,-1%"
copy /Y "%MINGW_BIN%\libwinpthread-1.dll" "%SCRIPT_DIR%build\"
copy /Y "%MINGW_BIN%\libgcc_s_seh-1.dll"  "%SCRIPT_DIR%build\" >nul 2>nul
copy /Y "%MINGW_BIN%\libstdc++-6.dll"     "%SCRIPT_DIR%build\" >nul 2>nul

if not exist "%SCRIPT_DIR%build\ui" mkdir "%SCRIPT_DIR%build\ui"
xcopy /E /Y /Q "%SCRIPT_DIR%src\ui_dist\*" "%SCRIPT_DIR%build\ui\" >nul

echo.
echo ============================================
echo  SUCCESS!
echo ============================================
echo.
set /p LAUNCH=Launch now? (y/n): 
if /i "%LAUNCH%"=="y" start "" "%SCRIPT_DIR%build\CrosshairG_v1.3.exe"
pause
