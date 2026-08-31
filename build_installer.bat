@echo off
setlocal EnableExtensions
cd /d "%~dp0"

call deploy.bat
if errorlevel 1 exit /b %errorlevel%

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 was not found.
    echo Install it from https://jrsoftware.org/isdl.php, then run this script again.
    exit /b 1
)

for /f "tokens=3" %%v in ('findstr /r /c:"project(z VERSION [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" CMakeLists.txt') do set "VERSION=%%v"
if "%VERSION%"=="" (
    echo ERROR: Could not read the app version from CMakeLists.txt.
    exit /b 1
)

if exist release_pkg rmdir /s /q release_pkg
mkdir release_pkg
xcopy /E /I /Y /Q dist release_pkg\ >nul
"%ISCC%" /DMyAppVersion=%VERSION% installer\Z.iss
if errorlevel 1 exit /b %errorlevel%

echo.
echo Installer created: Z-VideoEditor-Setup-Windows-x64.exe
exit /b 0