@echo off
if not exist "%~dp0Builds\Windows\Seige.exe" (
  echo Build the game first using Tools\build.ps1 -Package.
  pause
  exit /b 1
)
start "seige2222" "%~dp0Builds\Windows\Seige.exe" -windowed -ResX=1600 -ResY=900
