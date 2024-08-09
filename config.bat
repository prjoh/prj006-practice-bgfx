@echo off

REM Commands to configure this repo and dependencies for building

@REM rmdir /s /q "Temp/vs2022" 2>nul
@REM rmdir /s /q "Temp/ninja-debug" 2>nul
@REM rmdir /s /q "Temp/ninja-release" 2>nul

cmake -B Temp/vs2022 -G "Visual Studio 17 2022" -A x64

@REM ninja --version > nul 2>&1
@REM if %errorlevel% neq 0 (
@REM     echo Ninja is not installed. If you wish to build using Ninja, please install it and ensure it's in your PATH:
@REM     echo https://github.com/ninja-build/ninja/releases
@REM     exit /b 1
@REM )

@REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@REM cmake -B Temp/ninja-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
@REM cmake -B Temp/ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release

echo.
echo Press any key to exit . . .
pause >nul
