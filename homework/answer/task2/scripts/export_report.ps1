$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskReportName = -join @([char]0x4f5c,[char]0x4e1a,[char]0x62a5,[char]0x544a)
$taskInput = Join-Path $taskRoot "docs\$taskReportName.docx"
$taskOutput = Join-Path $taskRoot "docs\$taskReportName.pdf"
$taskWord = $null
try {
    $taskWord = New-Object -ComObject Word.Application
    $taskWord.Visible = $false
    $taskWord.DisplayAlerts = 0
    $taskDoc = $taskWord.Documents.Open($taskInput, $false, $true)
    try {
        $taskDoc.ExportAsFixedFormat($taskOutput, 17)
        $taskRecord = [ordered]@{
            application = "Word $($taskWord.Version)"
            opened = $true
            pages = $taskDoc.ComputeStatistics(2)
            docx = (Get-FileHash -LiteralPath $taskInput -Algorithm SHA256).Hash.ToLowerInvariant()
            pdf = (Get-FileHash -LiteralPath $taskOutput -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $taskAudit = Join-Path $taskRoot 'verification\report'
        New-Item -ItemType Directory -Force $taskAudit | Out-Null
        $taskRecord | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskAudit 'word-export.json') -Encoding UTF8
        Write-Output "Word export complete: $taskOutput ($($taskRecord.pages) pages)"
    } finally { $taskDoc.Close(0) }
} finally {
    if ($taskWord) {
        $taskWord.Quit()
        [Runtime.InteropServices.Marshal]::ReleaseComObject($taskWord) | Out-Null
    }
}
