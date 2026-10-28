@echo off
setlocal
rem Ghosts of November - full editor build (brief section 8).
rem Close the editor first: header changes + Live Coding crashed the prototype and ran stale code.
rem   UE_ROOT      default "J:\Unreal Multi\Editor\UE_5.7"
rem   GON_PROJECT  default: the .uproject next to this file

if "%UE_ROOT%"=="" set "UE_ROOT=J:\Unreal Multi\Editor\UE_5.7"
if "%GON_PROJECT%"=="" for %%f in ("%~dp0*.uproject") do set "GON_PROJECT=%%~ff"
if "%GON_PROJECT%"=="" (
  echo No .uproject next to build.bat. Set GON_PROJECT=C:\path\MyProject2.uproject
  exit /b 1
)
for %%f in ("%GON_PROJECT%") do set "GON_TARGET=%%~nfEditor"

tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
  echo Unreal Editor is running. Close it before a full build ^(never Live Coding after header changes^).
  exit /b 1
)

echo Building %GON_TARGET% from "%GON_PROJECT%"
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %GON_TARGET% Win64 Development "%GON_PROJECT%" -WaitMutex
set "GON_ERR=%ERRORLEVEL%"

echo.
echo GoN module binaries - compare with the newest source before diagnosing a report:
for %%m in ("%~dp0Plugins\GhostsOfNovember\Binaries\Win64\UnrealEditor-GoN*.dll") do echo   %%~tm  %%~nxm
powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0Plugins\GhostsOfNovember\Source' -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1 | ForEach-Object { '  newest source: ' + $_.LastWriteTime + '  ' + $_.Name }"
exit /b %GON_ERR%
