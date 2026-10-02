param([switch]$Test)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskCmake = Join-Path $taskRoot '.tools\cmake-3.31.10-windows-x86_64\bin\cmake.exe'
$taskCompilerDir = 'C:\msys64\mingw64\bin'
if (-not (Test-Path -LiteralPath $taskCmake)) { throw "Missing local CMake: $taskCmake" }
$env:PATH = "$taskCompilerDir;$env:PATH"
& $taskCmake -S "$taskRoot\code" -B "$taskRoot\build" -G 'MinGW Makefiles' `
  "-DCMAKE_C_COMPILER=$taskCompilerDir/gcc.exe" `
  "-DCMAKE_CXX_COMPILER=$taskCompilerDir/g++.exe" `
  "-DCMAKE_MAKE_PROGRAM=$taskCompilerDir/mingw32-make.exe" `
  "-DCMAKE_PREFIX_PATH=$taskRoot/.tools/freetype" `
  '-DCMAKE_BUILD_TYPE=RelWithDebInfo' '-DGLFW_BUILD_DOCS=OFF' `
  '-DHAVE_AVX_EXTENSIONS=OFF' '-DHAVE_AVX2_EXTENSIONS=OFF' '-DBUILD_VERIFICATION=ON'
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $taskCmake --build "$taskRoot\build" --parallel 6
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
New-Item -ItemType Directory -Path "$taskRoot\bin" -Force | Out-Null
Copy-Item -LiteralPath "$taskRoot\build\draw.exe" -Destination "$taskRoot\bin\draw.exe" -Force
# Package the exact runtime DLLs used by this compiler; OpenGL is supplied by Windows.
foreach ($taskDll in 'libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll') {
  Copy-Item -LiteralPath "$taskCompilerDir\$taskDll" -Destination "$taskRoot\bin" -Force
}
if ($Test) {
  & (Join-Path (Split-Path $taskCmake -Parent) 'ctest.exe') --test-dir "$taskRoot\build" --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw 'Algorithm verification failed' }
}
Write-Output "Ready: $taskRoot\bin\draw.exe"
