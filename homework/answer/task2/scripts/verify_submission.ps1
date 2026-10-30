param([string]$ExtractionParent = [IO.Path]::GetTempPath())
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$taskName = Split-Path $taskRoot -Leaf
$taskPackage = "$taskName-submit"
$taskZip = Join-Path $taskRoot "submission\$taskPackage.zip"
$taskAudit = Join-Path $taskRoot 'verification\submission-layout'
function Invoke-TaskRuntime([string]$Executable, [string[]]$Arguments, [string]$LogName) {
    $taskQuotedArgs = @($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' })
    $taskProcess = Start-Process -FilePath $Executable -ArgumentList $taskQuotedArgs -WorkingDirectory (Get-Location).Path `
        -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $taskAudit ($LogName + '.log')) `
        -RedirectStandardError (Join-Path $taskAudit ($LogName + '-stderr.log'))
    if ($taskProcess.ExitCode -ne 0) { throw "Runtime failed: $LogName (exit $($taskProcess.ExitCode))" }
}
$taskParent = (Resolve-Path -LiteralPath $ExtractionParent).Path
$taskExtract = Join-Path $taskParent ($taskName + '-check-' + [guid]::NewGuid().ToString('N').Substring(0,8))
if (Test-Path -LiteralPath $taskExtract) { throw 'Extraction folder must be new.' }
if (-not ([IO.Path]::GetFullPath($taskExtract)).StartsWith($taskParent.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid extraction path.' }
New-Item -ItemType Directory -Path $taskExtract -Force | Out-Null
Expand-Archive -LiteralPath $taskZip -DestinationPath $taskExtract
$taskCopy = Join-Path $taskExtract $taskPackage
$taskChecked = 0
foreach ($taskFile in Get-ChildItem -LiteralPath $taskCopy -File -Force -Recurse) {
    $taskRelative = $taskFile.FullName.Substring($taskCopy.Length + 1)
    if ($taskRelative -match '(?i)(^|\\)(scripts|verification|test|tests|testing|benchmarks|\.github)(\\|$)|(^|\\)testbuild\.sh$') {
        throw "Verification/support script included in submission: $taskRelative"
    }
    $taskOriginal = Join-Path $taskRoot $taskRelative
    if (-not (Test-Path -LiteralPath $taskOriginal -PathType Leaf)) { throw "Missing canonical source: $taskRelative" }
    if ((Get-FileHash -LiteralPath $taskFile.FullName).Hash -ne (Get-FileHash -LiteralPath $taskOriginal).Hash) { throw "Extracted file differs: $taskRelative" }
    $taskChecked++
}
$taskCmake = Get-ChildItem -LiteralPath (Join-Path $taskRoot '.tools') -Directory -Filter 'cmake*' |
    ForEach-Object { Join-Path $_.FullName 'bin\cmake.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $taskCmake) { $taskCmake = (Get-Command cmake -CommandType Application).Source }
New-Item -ItemType Directory -Path $taskAudit -Force | Out-Null
# Capture native stderr at the process boundary. PowerShell 5.1 otherwise turns
# ordinary compiler warnings into terminating errors when piping merged streams.
$taskBuildRunner = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$taskBuildArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    ('"' + (Join-Path $taskCopy 'build.ps1') + '"'), '-CMake', ('"' + $taskCmake + '"'))
$taskBuildProcess = Start-Process -FilePath $taskBuildRunner -ArgumentList $taskBuildArgs -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $taskAudit 'extracted-build.log') `
    -RedirectStandardError (Join-Path $taskAudit 'extracted-build-stderr.log')
if ($taskBuildProcess.ExitCode -ne 0) { throw 'Extracted build failed.' }
$taskOriginalPath = $env:PATH
$taskImages = Join-Path $taskCopy 'results'
New-Item -ItemType Directory -Path $taskImages -Force | Out-Null
try {
    $env:PATH = Join-Path $env:SystemRoot 'System32'
    if ($taskName -eq 'task2') {
        Push-Location (Join-Path $taskCopy 'code')
        try {
            Invoke-TaskRuntime (Join-Path $taskCopy 'bin\meshedit.exe') @('dae/cube.dae', '--ops', 'symmetric,loop,loop', '--screenshot', (Join-Path $taskImages 'cube.png')) 'extracted-run'
            Invoke-TaskRuntime (Join-Path $taskCopy 'bin\meshedit.exe') @('dae/custom/humanoid.dae', '--ops', 'hud-off,loop,loop,smooth,env', '--screenshot', (Join-Path $taskImages 'humanoid-env.png')) 'extracted-bonus-run'
        } finally { Pop-Location }
    } else {
        Push-Location $taskCopy
        try {
            Invoke-TaskRuntime (Join-Path $taskCopy 'bin\pathtracer.exe') @('-t', '4', '-s', '16', '-l', '4', '-m', '5', '-r', '160', '120', '-f', (Join-Path $taskImages 'spheres.png'), 'code/dae/sky/CBspheres_lambertian.dae') 'extracted-run'
            foreach ($taskBonus in @(@('materials', 'code/dae/sky/CBspheres.dae'), @('fzu', 'code/dae/fzu/FZU-gallery.dae'))) {
                Invoke-TaskRuntime (Join-Path $taskCopy 'bin\pathtracer.exe') @('-t', '4', '-s', '16', '-l', '4', '-m', '8', '-r', '160', '120', '-f', (Join-Path $taskImages ($taskBonus[0] + '.png')), $taskBonus[1]) ('extracted-' + $taskBonus[0] + '-run')
            }
        } finally { Pop-Location }
    }
} finally { $env:PATH = $taskOriginalPath }
$taskRecord = [ordered]@{
    package = $taskPackage
    archive_sha256 = (Get-FileHash -LiteralPath $taskZip).Hash.ToLowerInvariant()
    extracted_path = $taskCopy
    files_compared = $taskChecked
    build = 'passed'
    verification_files = 'excluded'
    runtime_path = 'Windows System32 only'
    runtime = 'passed'
    images = @(Get-ChildItem -LiteralPath $taskImages -File | Select-Object -ExpandProperty Name)
}
$taskRecord | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskAudit 'submission-verify.json') -Encoding UTF8
Write-Output "Verified $taskPackage from $taskZip"
