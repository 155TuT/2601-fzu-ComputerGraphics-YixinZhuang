param(
    [string]$CMake = '',
    [string]$CompilerDir = '',
    [ValidateRange(1, 64)][int]$Jobs = 6
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath (Join-Path $taskRoot 'code/verification/algorithm_tests.cpp') -PathType Leaf)) {
    throw 'Run scripts/test.ps1 from the complete source directory containing code/verification.'
}

function Resolve-TaskApplication([string]$Requested) {
    if (Test-Path -LiteralPath $Requested -PathType Leaf) {
        return (Resolve-Path -LiteralPath $Requested).Path
    }
    $taskCommand = Get-Command -Name $Requested -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($taskCommand) { return $taskCommand.Source }
    throw "Cannot find $Requested. Add it to PATH or pass its path to scripts/test.ps1."
}

if (-not $CMake) {
    $taskTools = Join-Path $taskRoot '.tools'
    if (Test-Path -LiteralPath $taskTools -PathType Container) {
        $taskLocalCmake = Get-ChildItem -LiteralPath $taskTools -Directory -Filter 'cmake*' |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName 'bin\cmake.exe' } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1
        if ($taskLocalCmake) { $CMake = $taskLocalCmake }
    }
    if (-not $CMake) { $CMake = 'cmake' }
}
$taskCmake = Resolve-TaskApplication $CMake
$taskCtest = Join-Path (Split-Path $taskCmake -Parent) 'ctest.exe'
if (-not (Test-Path -LiteralPath $taskCtest -PathType Leaf)) {
    $taskCtest = Resolve-TaskApplication 'ctest'
}

if (-not $CompilerDir) {
    $taskGpp = Get-Command -Name 'g++.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($taskGpp) {
        $CompilerDir = Split-Path -Path $taskGpp.Source -Parent
    } elseif (Test-Path -LiteralPath 'C:\msys64\mingw64\bin\g++.exe' -PathType Leaf) {
        $CompilerDir = 'C:\msys64\mingw64\bin'
    } else {
        throw 'Cannot find MinGW g++. Add its bin directory to PATH or use -CompilerDir.'
    }
}
$taskCompilerDir = (Resolve-Path -LiteralPath $CompilerDir).Path
foreach ($taskTool in 'gcc.exe', 'g++.exe', 'mingw32-make.exe') {
    if (-not (Test-Path -LiteralPath (Join-Path $taskCompilerDir $taskTool) -PathType Leaf)) {
        throw "Missing $taskTool in $taskCompilerDir. Use a complete 64-bit MinGW toolchain."
    }
}

$taskBuild = Join-Path $taskRoot 'build-test'
$taskOriginalPath = $env:PATH
try {
    $env:PATH = "$taskCompilerDir;$taskOriginalPath"
    $taskConfigureArgs = @(
        '-S', (Join-Path $taskRoot 'code'),
        '-B', $taskBuild,
        '-G', 'MinGW Makefiles',
        "-DCMAKE_C_COMPILER=$(Join-Path $taskCompilerDir 'gcc.exe')",
        "-DCMAKE_CXX_COMPILER=$(Join-Path $taskCompilerDir 'g++.exe')",
        "-DCMAKE_MAKE_PROGRAM=$(Join-Path $taskCompilerDir 'mingw32-make.exe')",
        '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        '-DGLFW_BUILD_DOCS=OFF',
        '-DHAVE_AVX_EXTENSIONS=OFF',
        '-DHAVE_AVX2_EXTENSIONS=OFF',
        '-DBUILD_VERIFICATION=ON'
    )
    & $taskCmake @taskConfigureArgs
    if ($LASTEXITCODE -ne 0) { throw 'Test configuration failed.' }
    & $taskCmake --build $taskBuild --parallel $Jobs --target algorithm_tests material_tests
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    & $taskCtest --test-dir $taskBuild --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Algorithm or material verification failed.' }
    Write-Output 'Algorithm and material verification passed.'
} finally {
    $env:PATH = $taskOriginalPath
}
