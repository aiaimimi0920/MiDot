$ErrorActionPreference = 'Stop'
$stage = Join-Path (Get-Location) 'export.stage-e5e72dc04-20260923-editor-contrast'
if (-not (Test-Path -LiteralPath $stage)) { throw "Missing stage: $stage" }
foreach ($file in Get-ChildItem -LiteralPath $stage -File) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path 'export' $file.Name) -Force
}
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Output 'export switched'
