param([string]$Svg = 'code\svg\basic\test4.svg')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskSvg = if ([System.IO.Path]::IsPathRooted($Svg)) { $Svg } else { Join-Path $taskRoot $Svg }
Push-Location $taskRoot
try { & "$taskRoot\bin\draw.exe" $taskSvg } finally { Pop-Location }
