@echo off
REM ==============================================================
REM  Build script for Amar Bangla Kotha Windows TSF Engine DLL
REM ==============================================================

echo Building Amar Bangla Kotha Windows TSF TIP...

mkdir build 2>nul
cd build

cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo CMake generation failed! Trying ninja / nmake...
    cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
)

cmake --build . --config Release

if %ERRORLEVEL% EQU 0 (
    echo =========================================================
    echo Build successful!
    echo Output DLL: bin\Release\abk_tsf.dll (or Release\abk_tsf.dll)
    echo To register the Text Service:
    echo   regsvr32 Release\abk_tsf.dll
    echo To unregister:
    echo   regsvr32 /u Release\abk_tsf.dll
    echo =========================================================
) else (
    echo Build failed with error code %ERRORLEVEL%.
)

cd ..
