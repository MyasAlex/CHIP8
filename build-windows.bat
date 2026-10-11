@echo off
setlocal
cd /d "%~dp0"
set "CHIP8_MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "CHIP8_MSBUILD=%%i"
if not defined CHIP8_MSBUILD (
  echo Visual Studio with the Desktop development with C++ workload is required.
  pause
  exit /b 1
)
"%CHIP8_MSBUILD%" CHIP8.sln /m /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
  pause
  exit /b 1
)
bin\x64\Release\CHIP8.Tests.exe
if errorlevel 1 (
  pause
  exit /b 1
)
start "" bin\x64\Release\CHIP8.Emulator.exe
