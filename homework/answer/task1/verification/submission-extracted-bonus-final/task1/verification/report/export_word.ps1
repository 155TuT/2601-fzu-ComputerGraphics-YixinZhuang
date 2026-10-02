param(
  [string]$Docx = "homework/answer/task1/docs/作业报告.docx",
  [string]$Pdf = "homework/answer/task1/docs/作业报告.pdf",
  [string]$Audit = "homework/answer/task1/verification/report/word-open-audit.json"
)
$ErrorActionPreference = "Stop"
$reportWord = $null
$reportDoc = $null
$reportFailure = $null
try {
  $reportWord = New-Object -ComObject Word.Application
  $reportWord.Visible = $false
  $reportWord.DisplayAlerts = 0
  $reportWord.AutomationSecurity = 3
  $reportDocPath = (Resolve-Path -LiteralPath $Docx).Path
  $reportPdfPath = [System.IO.Path]::GetFullPath($Pdf)
  $reportAuditPath = [System.IO.Path]::GetFullPath($Audit)
  $reportDoc = $reportWord.Documents.Open($reportDocPath, $false, $true, $false)
  $reportDoc.Repaginate()
  $reportDoc.ExportAsFixedFormat($reportPdfPath, 17)
  $reportResult = [ordered]@{
    app = "Microsoft Word"
    version = $reportWord.Version
    read_only = $reportDoc.ReadOnly
    pages = $reportDoc.ComputeStatistics(2)
    paragraphs = $reportDoc.Paragraphs.Count
    inline_shapes = $reportDoc.InlineShapes.Count
    docx = $reportDocPath
    pdf = $reportPdfPath
    normal_open_succeeded = $true
    export_pdf_succeeded = (Test-Path -LiteralPath $reportPdfPath)
  }
  $reportResult | ConvertTo-Json | Set-Content -LiteralPath $reportAuditPath -Encoding utf8
  $reportResult | ConvertTo-Json
} catch {
  $reportFailure = $_
} finally {
  $reportNoSave = 0
  if ($null -ne $reportDoc) { $reportDoc.Close([ref]$reportNoSave) }
  if ($null -ne $reportWord) {
    $reportWord.Quit([ref]$reportNoSave)
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($reportWord) | Out-Null
  }
}
if ($null -ne $reportFailure) { throw $reportFailure }
