@echo off
setlocal
rem Ghosts of November - run every "GoN.*" Unreal automation test headless (brief section 8).
rem Build first (build.bat). Report: Saved\GoNTestReport\index.json, log: Saved\Logs.
rem Default filter "StartsWith:GoN." (UE 5.2+ prefix match): a plain "GoN" also matched engine tests such as "Polygon".
rem   run_tests.bat StartsWith:GoN.Narrative.   or any other filter, e.g. run_tests.bat GoN.Blockout

if "%UE_ROOT%"=="" set "UE_ROOT=J:\Unreal Multi\Editor\UE_5.7"
if "%GON_PROJECT%"=="" for %%f in ("%~dp0*.uproject") do set "GON_PROJECT=%%~ff"
if "%GON_PROJECT%"=="" (
  echo No .uproject next to run_tests.bat. Set GON_PROJECT=C:\path\MyProject2.uproject
  exit /b 1
)
set "GON_FILTER=%~1"
if "%GON_FILTER%"=="" set "GON_FILTER=StartsWith:GoN."

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%GON_PROJECT%" -ExecCmds="Automation RunTests %GON_FILTER%; Quit" -unattended -nullrhi -nosplash -nopause -log -ReportExportPath="%~dp0Saved\GoNTestReport"
exit /b %ERRORLEVEL%
