@echo off
setlocal
rem Copy only the installed Opposing Force client into this separate mod.
rem Never overwrite an existing client or touch valve, gearbox, or SAVE.
set "MOD=%~dp0"
set "SOURCE=%MOD%..\gearbox\cl_dlls\client.dll"
set "GAME_DLL=%MOD%..\gearbox\dlls\opfor.dll"
set "TARGET_DIR=%MOD%cl_dlls"
set "TARGET=%TARGET_DIR%\client.dll"
if not exist "%GAME_DLL%" (
  echo ERROR: gearbox\dlls\opfor.dll not found. Install Opposing Force in the same Half-Life folder.
  goto fail
)
if not exist "%SOURCE%" (
  echo ERROR: gearbox\cl_dlls\client.dll not found. Verify the Opposing Force installation.
  goto fail
)
if not exist "%MOD%dlls\hl.dll" (
  echo ERROR: this mod's dlls\hl.dll is missing.
  goto fail
)
if exist "%TARGET%" (
  fc /b "%SOURCE%" "%TARGET%" >nul
  if errorlevel 1 (
    echo ERROR: cl_dlls\client.dll already exists and differs. Back it up and replace it manually if desired.
    goto fail
  )
  echo Opposing Force client is already installed and matches.
  goto success
)
if not exist "%TARGET_DIR%\" mkdir "%TARGET_DIR%"
if not exist "%TARGET_DIR%\" (
  echo ERROR: could not create cl_dlls.
  goto fail
)
copy /b "%SOURCE%" "%TARGET%" >nul
if errorlevel 1 (
  echo ERROR: could not copy the Opposing Force client.
  goto fail
)
fc /b "%SOURCE%" "%TARGET%" >nul
if errorlevel 1 (
  echo ERROR: copied client does not match. Check the files manually.
  goto fail
)
echo Opposing Force client copied. Launch Half-Life with -game hl_randomizer_opfor.
:success
if exist "%MOD%install_hev_hud.exe" (
  echo Preparing optional Half-Life health/armor sprites from local game files...
  "%MOD%install_hev_hud.exe"
  if errorlevel 1 echo HUD overlay was not installed; the Opposing Force client remains usable.
)
if not defined CHAOS_INSTALL_NO_PAUSE pause
exit /b 0
:fail
if not defined CHAOS_INSTALL_NO_PAUSE pause
exit /b 1
