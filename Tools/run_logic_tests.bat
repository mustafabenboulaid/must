@echo off
setlocal
rem Build and run the pure C++ logic tests without Unreal (CMake 3.20+ and Visual Studio 2022).
rem   Tools\run_logic_tests.bat             all tests, normal + unity/UE-macro build
rem   Tools\run_logic_tests.bat --validate  print every data issue in Data/Narrative
set "ROOT=%~dp0.."
cmake -S "%ROOT%\Tools\LogicTests" -B "%ROOT%\Build\LogicTests" || exit /b 1
cmake --build "%ROOT%\Build\LogicTests" --config Debug || exit /b 1
if "%~1"=="--validate" (
  "%ROOT%\Build\LogicTests\Debug\gon_logic_tests.exe" --validate
  exit /b %ERRORLEVEL%
)
"%ROOT%\Build\LogicTests\Debug\gon_logic_tests.exe" %* || exit /b 1
"%ROOT%\Build\LogicTests\Debug\gon_logic_tests_unity.exe" %*
exit /b %ERRORLEVEL%
