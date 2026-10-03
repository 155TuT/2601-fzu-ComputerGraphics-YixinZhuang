@echo off
setlocal
pushd "%~dp0"
set "taskSvg=%~1"
if not defined taskSvg set "taskSvg=code\svg\basic\test4.svg"
"bin\draw.exe" "%taskSvg%"
set "taskExit=%ERRORLEVEL%"
popd
if not "%taskExit%"=="0" pause
exit /b %taskExit%
