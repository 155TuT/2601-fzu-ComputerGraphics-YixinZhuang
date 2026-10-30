param(
    [string]$CMake = '',
    [string]$CompilerDir = '',
    [ValidateRange(1, 64)][int]$Jobs = 6
)

$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot

function Resolve-TaskApplication([string]$Requested) {
    if (Test-Path -LiteralPath $Requested -PathType Leaf) {
        return (Resolve-Path -LiteralPath $Requested).Path
    }
    $taskCommand = Get-Command -Name $Requested -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($taskCommand) { return $taskCommand.Source }
    throw "Cannot find $Requested. Add it to PATH or pass its path to build.ps1."
}

# Development can use its task-local CMake; the submitted copy also works
# with CMake on PATH without carrying the development tools directory.
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

$taskBuild = Join-Path $taskRoot 'build'
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
        '-DBUILD_VERIFICATION=OFF'
    )
    & $taskCmake @taskConfigureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

    $taskBuildArgs = @('--build', $taskBuild, '--parallel', $Jobs, '--target', 'pathtracer')
    & $taskCmake @taskBuildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }

    $taskBin = Join-Path $taskRoot 'bin'
    New-Item -ItemType Directory -Path $taskBin -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskBuild 'pathtracer.exe') -Destination (Join-Path $taskBin 'pathtracer.exe') -Force
    # Package the runtime DLLs from the selected compiler; Windows supplies OpenGL.
    foreach ($taskDll in 'libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll') {
        Copy-Item -LiteralPath (Join-Path $taskCompilerDir $taskDll) -Destination $taskBin -Force
    }

    Write-Output "Ready: $taskBin\pathtracer.exe"
} finally {
    $env:PATH = $taskOriginalPath
}

