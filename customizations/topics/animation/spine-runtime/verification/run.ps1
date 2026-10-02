[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$GodotExecutable
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$customizations = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..\.."))
if ([string]::IsNullOrWhiteSpace($EnginePath)) {
    $EnginePath = Join-Path $customizations "..\engine"
}
$engine = [System.IO.Path]::GetFullPath($EnginePath)

if ([string]::IsNullOrWhiteSpace($GodotExecutable)) {
    $GodotExecutable = Join-Path $engine "bin\godot.windows.editor.dev.x86_64.console.exe"
}
$godot = [System.IO.Path]::GetFullPath($GodotExecutable)
if (-not (Test-Path -LiteralPath $godot -PathType Leaf)) {
    throw "Godot editor executable not found: $godot"
}

$tempRoot = [System.IO.Path]::GetFullPath((Join-Path $customizations ".tmp"))
$project = [System.IO.Path]::GetFullPath((Join-Path $tempRoot "spine-runtime-smoke"))
$expectedPrefix = $tempRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $project.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to manage a smoke project outside the customization temp root: $project"
}

function Get-MatchingGodotProcesses {
    @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        try {
            $_.Path -and ([System.IO.Path]::GetFullPath($_.Path) -eq $godot)
        } catch {
            $false
        }
    })
}

function Invoke-SpineSmoke {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogName
    )

    $log = Join-Path $project $LogName
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $godot @Arguments *> $log
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $log -Encoding Unicode
        throw "Godot smoke failed with exit code ${exitCode}: $LogName"
    }
}

$baselineIds = @(Get-MatchingGodotProcesses | Select-Object -ExpandProperty Id)
$leaked = @()
try {
    if (Test-Path -LiteralPath $project) {
        Remove-Item -LiteralPath $project -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $project | Out-Null
    foreach ($file in @("project.godot", "smoke.gd", "invalid_load.gd", "atlas_load.gd", "empty.spskel", "valid.atlas", "missing.atlas", "page.svg")) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $project $file)
    }

    Invoke-SpineSmoke -Arguments @("--headless", "--editor", "--path", $project, "--quit") -LogName "editor-smoke.log"
    Invoke-SpineSmoke -Arguments @("--headless", "--path", $project, "--script", "res://smoke.gd") -LogName "class-smoke.log"
    Invoke-SpineSmoke -Arguments @("--headless", "--path", $project, "--script", "res://invalid_load.gd") -LogName "invalid-load.log"
    Invoke-SpineSmoke -Arguments @("--headless", "--path", $project, "--script", "res://atlas_load.gd") -LogName "atlas-load.log"

    $classLog = Get-Content -LiteralPath (Join-Path $project "class-smoke.log") -Raw -Encoding Unicode
    $invalidLog = Get-Content -LiteralPath (Join-Path $project "invalid-load.log") -Raw -Encoding Unicode
    $atlasLog = Get-Content -LiteralPath (Join-Path $project "atlas-load.log") -Raw -Encoding Unicode
    if ($classLog.IndexOf("SPINE_SMOKE_OK", [System.StringComparison]::Ordinal) -lt 0) {
        throw "Class smoke success marker is missing."
    }
    if ($invalidLog.IndexOf("SPINE_INVALID_LOAD_REJECTED", [System.StringComparison]::Ordinal) -lt 0) {
        throw "Invalid-load success marker is missing."
    }
    if ($atlasLog.IndexOf("SPINE_ATLAS_LOAD_OK", [System.StringComparison]::Ordinal) -lt 0) {
        throw "Atlas-load success marker is missing."
    }
} finally {
    $leaked = @(Get-MatchingGodotProcesses | Where-Object { $baselineIds -notcontains $_.Id })
    foreach ($process in $leaked) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $project) {
        Remove-Item -LiteralPath $project -Recurse -Force
    }
}

if ($leaked.Count -ne 0) {
    throw "Cleaned $($leaked.Count) leaked Godot smoke process(es)."
}

Write-Output "Spine runtime smoke passed."
