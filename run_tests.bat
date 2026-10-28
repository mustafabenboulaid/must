@echo off
setlocal
rem Ghosts of November - run every "GoN.*" Unreal automation test headless (brief section 8).
rem Build first (build.bat). Report: Saved\GoNTestReport\index.json, log: Saved\Logs.

if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
if "%GON_PROJECT%"=="" for %%f in ("%~dp0*.uproject") do set "GON_PROJECT=%%~ff"
if "%GON_PROJECT%"=="" (
  echo No .uproject next to run_tests.bat. Set GON_PROJECT=C:\path\MyProject2.uproject
  exit /b 1
)
set "GON_FILTER=%~1"
if "%GON_FILTER%"=="" set "GON_FILTER=GoN"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%GON_PROJECT%" -ExecCmds="Automation RunTests %GON_FILTER%; Quit" -unattended -nullrhi -nosplash -nopause -log -ReportExportPath="%~dp0Saved\GoNTestReport"
exit /b %ERRORLEVEL%
