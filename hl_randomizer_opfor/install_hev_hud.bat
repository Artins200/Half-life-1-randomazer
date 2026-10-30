@echo off
setlocal
if not exist "%~dp0install_hev_hud.exe" (
  echo ERROR: install_hev_hud.exe is missing from the mod folder.
  if not defined CHAOS_INSTALL_NO_PAUSE pause
  exit /b 1
)
"%~dp0install_hev_hud.exe"
set "RESULT=%ERRORLEVEL%"
if not defined CHAOS_INSTALL_NO_PAUSE pause
exit /b %RESULT%
