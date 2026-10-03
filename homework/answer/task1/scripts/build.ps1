param(
    [switch]$Test,
    [string]$CMake = '',
    [string]$CompilerDir = '',
    [ValidateRange(1, 64)][int]$Jobs = 6
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
# Compatibility entry point for the course's VS Code tasks: build every
# verification program for development, using the same portable build script.
& (Join-Path $taskRoot 'build.ps1') -CMake $CMake -CompilerDir $CompilerDir -Jobs $Jobs -Verification -Test:$Test
