@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MINGW=C:\msys64\mingw64\bin"
set "MSYS=C:\msys64\usr\bin"
set "QT_BIN=C:\msys64\mingw64\share\qt6\bin"
set "BUILD=build-deploy"
set "EXE=%BUILD%\bin\z.exe"
set "OUT=dist"
set "PATH=%MINGW%;%QT_BIN%;%MSYS%;%PATH%"
set "QT_PLUGIN_PATH=C:\msys64\mingw64\share\qt6\plugins"

if not exist "%MINGW%\c++.exe" (
    echo ERROR: MSYS2 MinGW64 compiler was not found at "%MINGW%".
    exit /b 1
)
if not exist "%MINGW%\windeployqt6.exe" (
    echo ERROR: windeployqt6.exe was not found at "%MINGW%".
    exit /b 1
)

echo === Building latest Z Video Editor ===
if exist "%BUILD%" rmdir /s /q "%BUILD%"
cmake -S . -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%MINGW%\c++.exe" -DCMAKE_PREFIX_PATH="C:\msys64\mingw64"
if errorlevel 1 (
    echo ERROR: configure failed.
    exit /b 1
)

cmake --build "%BUILD%" --parallel
if errorlevel 1 (
    echo ERROR: build failed.
    exit /b 1
)

if not exist "%EXE%" (
    echo ERROR: %EXE% not found after build.
    exit /b 1
)

echo === Bundling Z Video Editor with all required DLLs ===
if exist %OUT% rmdir /s /q %OUT%
mkdir "%OUT%"

copy /Y "%EXE%" "%OUT%\" >nul
if errorlevel 1 (
    echo ERROR: failed to copy the application executable.
    exit /b 1
)

xcopy /E /I /Y /Q plugins "%OUT%\plugins\" >nul
xcopy /E /I /Y /Q docs "%OUT%\docs\" >nul
xcopy /E /I /Y /Q media "%OUT%\media\" >nul

echo Running windeployqt6...
"%MINGW%\windeployqt6.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw --dir "%OUT%" "%OUT%\z.exe"
if errorlevel 1 (
    echo ERROR: Qt deployment failed.
    exit /b 1
)

echo Copying FFmpeg DLLs...
copy /Y "%MINGW%\ffmpeg.exe" "%OUT%\" >nul 2>&1
for %%f in ("%MINGW%\avcodec*.dll" "%MINGW%\avformat*.dll" "%MINGW%\avutil*.dll" "%MINGW%\swscale*.dll" "%MINGW%\swresample*.dll" "%MINGW%\avdevice*.dll" "%MINGW%\avfilter*.dll") do (
    copy /Y "%%~f" "%OUT%\" >nul 2>&1
)

copy /Y "%MINGW%\libportaudio*.dll" "%OUT%\" >nul 2>&1
copy /Y "%MINGW%\libsqlite3*.dll" "%OUT%\" >nul 2>&1

echo Copying OpenCV YOLO inference DLLs...
for %%f in ("%MINGW%\libopencv_core*.dll" "%MINGW%\libopencv_dnn*.dll" "%MINGW%\libopencv_imgproc*.dll") do (
    copy /Y "%%~f" "%OUT%\" >nul 2>&1
)

copy /Y "%MINGW%\libgcc_s_seh-1.dll" "%OUT%\" >nul 2>&1
copy /Y "%MINGW%\libstdc++-6.dll" "%OUT%\" >nul 2>&1
copy /Y "%MINGW%\libwinpthread-1.dll" "%OUT%\" >nul 2>&1

echo Collecting additional native runtime dependencies...
for /f "tokens=3" %%d in ('ldd "%OUT%\z.exe" ^| findstr /i /c:"/mingw64/"') do (
    if exist "%%d" copy /Y "%%d" "%OUT%\" >nul 2>&1
)

echo.
echo === Done! Fresh distributable build is in: %OUT%\ ===
echo Launching %OUT%\z.exe...
start "" "%OUT%\z.exe"
exit /b 0
