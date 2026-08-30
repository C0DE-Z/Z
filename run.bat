@echo off
cd /d "%~dp0"
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
set QT_PLUGIN_PATH=C:\msys64\mingw64\share\qt6\plugins
if not exist build\bin\z.exe (
    echo Building the application first...
    cmake -S . -B build -G "Ninja" -DCMAKE_CXX_COMPILER="C:/msys64/mingw64/bin/c++.exe" -DCMAKE_C_COMPILER="C:/msys64/mingw64/bin/gcc.exe" -DCMAKE_PREFIX_PATH="C:/msys64/mingw64"
    cmake --build build
)
echo Launching z...
build\bin\z.exe
if %ERRORLEVEL% neq 0 (
    echo.
    echo z exited with error code %ERRORLEVEL%
    pause
)
