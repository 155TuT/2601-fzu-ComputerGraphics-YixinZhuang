# Rebuild the submission from canonical task1 files without transformations.
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$taskOutput = Join-Path $taskRoot 'submission'
$taskDelivery = Join-Path $taskOutput 'task1-submit'
$taskArchive = Join-Path $taskOutput 'task1-submit.zip'
$taskAudit = Join-Path $taskRoot 'verification\submission-layout'
$taskStage = Join-Path $taskOutput ('.package-' + [guid]::NewGuid().ToString('N'))
$taskStageDelivery = Join-Path $taskStage 'task1-submit'

function Assert-TaskOutputPath([string]$Path) {
    $taskFull = [IO.Path]::GetFullPath($Path)
    if (-not $taskFull.StartsWith($taskOutput + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Output path is outside the submission directory: $Path"
    }
}

function Get-TaskFiles([string]$Folder) {
    # Prune generated directories, rather than walking developer caches.
    foreach ($taskItem in Get-ChildItem -LiteralPath $Folder -Force) {
        if ($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Linked files/directories are not accepted: $($taskItem.FullName)"
        }
        if ($taskItem.PSIsContainer) {
            if ($taskItem.Name -notin @('.git', '__pycache__', '.capture', 'outputs')) {
                Get-TaskFiles $taskItem.FullName
            }
        } elseif ($taskItem.Name -ne '.DS_Store' -and $taskItem.Extension -ne '.pyc') {
            $taskItem
        }
    }
}

$taskFiles = @()
foreach ($taskName in @('README.md', '.gitignore', 'run.cmd', 'build.ps1')) {
    $taskFiles += Get-Item -LiteralPath (Join-Path $taskRoot $taskName)
}
foreach ($taskFolder in @('.vscode', 'code', 'docs', 'bin')) {
    $taskFiles += @(Get-TaskFiles (Join-Path $taskRoot $taskFolder))
}
$taskFiles = @($taskFiles | Sort-Object FullName)
$taskRelativeFiles = @{}
foreach ($taskFile in $taskFiles) {
    $taskRelative = $taskFile.FullName.Substring($taskRoot.Length + 1).Replace('\', '/')
    $taskRelativeFiles[$taskRelative] = $taskFile
}
# Codepoints keep this script usable in Windows PowerShell 5.1 without a BOM.
$taskReportBase = -join @([char]0x4f5c, [char]0x4e1a, [char]0x62a5, [char]0x544a)
foreach ($taskRequired in @('bin/draw.exe', 'bin/libstdc++-6.dll', 'bin/libgcc_s_seh-1.dll',
    'bin/libwinpthread-1.dll', 'code/src/rasterizer.cpp', 'code/src/transforms.cpp',
    'code/src/texture.cpp', 'code/CGL/deps/freetype/CMakeLists.txt',
    'code/verification/algorithm_tests.cpp', 'docs/my_robot.svg',
    'docs/texture_demo.svg', 'docs/sampling_texture.png',
    "docs/$taskReportBase.docx", "docs/$taskReportBase.pdf")) {
    if (-not $taskRelativeFiles.ContainsKey($taskRequired) -or
        $taskRelativeFiles[$taskRequired].Length -eq 0) {
        throw "Missing or empty canonical deliverable: $taskRequired"
    }
}

if (Test-Path -LiteralPath $taskOutput) {
    if ((Get-Item -LiteralPath $taskOutput).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Submission output must not be a linked directory: $taskOutput"
    }
}
if (Test-Path -LiteralPath $taskDelivery) {
    Assert-TaskOutputPath (Resolve-Path -LiteralPath $taskDelivery).Path
    if ((Get-Item -LiteralPath $taskDelivery).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Submission must not be a linked directory: $taskDelivery"
    }
    # Preserve work added only to the submission: require its promotion first.
    foreach ($taskExisting in Get-ChildItem -LiteralPath $taskDelivery -Recurse -Force) {
        if ($taskExisting.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Linked submission item is not accepted: $($taskExisting.FullName)"
        }
        if ($taskExisting.PSIsContainer) { continue }
        $taskRelative = $taskExisting.FullName.Substring($taskDelivery.Length + 1).Replace('\', '/')
        if ($taskRelative -match '^(build|results)/|^(test\.png|screenshot_.*\.png)$') { continue }
        if (-not $taskRelativeFiles.ContainsKey($taskRelative)) {
            throw "Submission-only file must be saved in task1 first: $taskRelative"
        }
    }
}

New-Item -ItemType Directory -Path $taskStageDelivery, $taskAudit -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskRecords = @()
try {
    foreach ($taskRelative in ($taskRelativeFiles.Keys | Sort-Object)) {
        $taskSource = $taskRelativeFiles[$taskRelative]
        $taskTarget = Join-Path $taskStageDelivery $taskRelative
        New-Item -ItemType Directory -Path (Split-Path $taskTarget -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $taskSource.FullName -Destination $taskTarget
        $taskHash = (Get-FileHash -LiteralPath $taskSource.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if ((Get-FileHash -LiteralPath $taskTarget -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskHash) {
            throw "Canonical file changed while copying: $taskRelative"
        }
        $taskRecords += [ordered]@{path=$taskRelative; bytes=$taskSource.Length; sha256=$taskHash}
    }

    $taskStageArchive = Join-Path $taskStage 'task1-submit.zip'
    $taskZip = [IO.Compression.ZipFile]::Open($taskStageArchive, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($taskRecord in $taskRecords) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($taskZip,
                (Join-Path $taskStageDelivery $taskRecord.path), "task1-submit/$($taskRecord.path)",
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $taskZip.Dispose() }

    $taskZip = [IO.Compression.ZipFile]::OpenRead($taskStageArchive)
    try {
        if ($taskZip.Entries.Count -ne $taskRecords.Count) { throw 'Archive file count mismatch.' }
        foreach ($taskRecord in $taskRecords) {
            $taskEntry = $taskZip.GetEntry("task1-submit/$($taskRecord.path)")
            if (-not $taskEntry -or $taskEntry.Length -ne $taskRecord.bytes) {
                throw "Archive member missing or wrong size: $($taskRecord.path)"
            }
            $taskStream = $taskEntry.Open()
            $taskHasher = [Security.Cryptography.SHA256]::Create()
            try { $taskHash = ([BitConverter]::ToString($taskHasher.ComputeHash($taskStream))).Replace('-', '').ToLowerInvariant() }
            finally { $taskStream.Dispose(); $taskHasher.Dispose() }
            if ($taskHash -ne $taskRecord.sha256) { throw "Archive hash mismatch: $($taskRecord.path)" }
        }
    } finally { $taskZip.Dispose() }

    $taskManifest = [ordered]@{
        schema=1; package_root='task1-submit'; generated_at=[DateTimeOffset]::Now.ToString('o')
        archive_sha256=(Get-FileHash -LiteralPath $taskStageArchive -Algorithm SHA256).Hash.ToLowerInvariant()
        archive_bytes=(Get-Item -LiteralPath $taskStageArchive).Length
        file_count=$taskRecords.Count; files=@($taskRecords)
    }
    # All removed/replaced paths are resolved within task1/submission/.
    foreach ($taskGenerated in @($taskDelivery, $taskArchive)) {
        Assert-TaskOutputPath $taskGenerated
        if (Test-Path -LiteralPath $taskGenerated) {
            Assert-TaskOutputPath (Resolve-Path -LiteralPath $taskGenerated).Path
            if ((Get-Item -LiteralPath $taskGenerated).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing to replace linked output: $taskGenerated"
            }
            Remove-Item -LiteralPath $taskGenerated -Recurse -Force
        }
    }
    Move-Item -LiteralPath $taskStageDelivery -Destination $taskDelivery
    Move-Item -LiteralPath $taskStageArchive -Destination $taskArchive
    [IO.File]::WriteAllText((Join-Path $taskAudit 'package-manifest.json'),
        ($taskManifest | ConvertTo-Json -Depth 8), (New-Object Text.UTF8Encoding($false)))
    Write-Output "Created: $taskArchive ($($taskRecords.Count) files)"
} finally {
    Assert-TaskOutputPath $taskStage
    if (Test-Path -LiteralPath $taskStage) {
        Assert-TaskOutputPath (Resolve-Path -LiteralPath $taskStage).Path
        Remove-Item -LiteralPath $taskStage -Recurse -Force
    }
}
