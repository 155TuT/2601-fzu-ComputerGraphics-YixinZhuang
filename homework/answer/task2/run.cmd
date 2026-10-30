@echo off
setlocal
set "SCENE=%~1"
if "%SCENE%"=="" set "SCENE=%~dp0code\dae\teapot.dae"
if not "%~1"=="" for %%I in ("%~1") do set "SCENE=%%~fI"
pushd "%~dp0code"
"%~dp0bin\meshedit.exe" "%SCENE%"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
