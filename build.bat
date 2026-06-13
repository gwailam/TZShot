@echo off
setlocal enableextensions

REM ============================================================
REM  TZShot build script (ASCII only, no localized comments,
REM  to avoid codepage corruption when cmd parses this file).
REM  It loads the MSVC x64 dev environment via vswhere, then
REM  runs the configured Ninja build and prints the log tail.
REM ============================================================

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [build] vswhere.exe not found. Is Visual Studio 2017+ installed?
  exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo [build] No Visual Studio installation with the C++ x64 toolset was found.
  exit /b 1
)

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo [build] vcvars64.bat not found at: "%VCVARS%"
  exit /b 1
)

call "%VCVARS%" >nul 2>&1

REM Sanity check: the C++ compiler must now be on PATH.
where cl.exe >nul 2>&1
if errorlevel 1 (
  echo [build] cl.exe is still not on PATH after loading vcvars64. Aborting.
  exit /b 1
)

cmake --build "%~dp0build-dev-ninja" > "%~dp0build-dev-ninja\build_log.txt" 2>&1
set RC=%ERRORLEVEL%
echo ---- BUILD EXIT CODE: %RC% ----
powershell -NoProfile -Command "Get-Content '%~dp0build-dev-ninja\build_log.txt' -Tail 40"
exit /b %RC%
