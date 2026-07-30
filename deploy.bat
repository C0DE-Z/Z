@echo off
setlocal

set MINGW=C:\msys64\mingw64\bin
set EXE=build\bin\z.exe
set OUT=dist

echo === Bundling Z Video Editor with all required DLLs ===

if not exist %EXE% (
    echo ERROR: %EXE% not found. Run cmake --build build first.
    exit /b 1
)

:: Clean and create output folder
if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%

:: Copy exe
copy %EXE% %OUT%\

:: Copy plugins and docs
xcopy /E /I /Q plugins %OUT%\plugins\
xcopy /E /I /Q docs %OUT%\docs\
xcopy /E /I /Q media %OUT%\media\

:: Run windeployqt6 to auto-gather Qt DLLs
echo Running windeployqt6...
%MINGW%\windeployqt6.exe --no-translations --no-system-d3d-compiler --no-opengl-sw --dir %OUT% %OUT%\z.exe

:: FFmpeg DLLs
echo Copying FFmpeg DLLs...
for %%f in (%MINGW%\avcodec*.dll %MINGW%\avformat*.dll %MINGW%\avutil*.dll %MINGW%\swscale*.dll %MINGW%\swresample*.dll %MINGW%\avdevice*.dll %MINGW%\avfilter*.dll) do (
    copy "%%f" %OUT%\ >nul 2>&1
)

:: PortAudio
copy %MINGW%\libportaudio*.dll %OUT%\ >nul 2>&1

:: SQLite
copy %MINGW%\libsqlite3*.dll %OUT%\ >nul 2>&1

:: MinGW runtimes
copy %MINGW%\libgcc_s_seh-1.dll %OUT%\ >nul 2>&1
copy %MINGW%\libstdc++-6.dll %OUT%\ >nul 2>&1
copy %MINGW%\libwinpthread-1.dll %OUT%\ >nul 2>&1

echo.
echo === Done! Distributable build is in: %OUT%\ ===
echo Run %OUT%\z.exe to launch.
