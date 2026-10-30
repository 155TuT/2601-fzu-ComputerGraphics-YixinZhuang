@echo off
setlocal
pushd "%~dp0"
if "%~1"=="" (
  bin\pathtracer.exe -t 4 code/dae/sky/CBspheres_lambertian.dae
) else (
  bin\pathtracer.exe %*
)
set "taskResult=%errorlevel%"
popd
exit /b %taskResult%
