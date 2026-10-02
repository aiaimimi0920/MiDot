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
$spoutDll = Join-Path (Split-Path -Parent $godot) "SpoutLibrary.dll"
if (-not (Test-Path -LiteralPath $spoutDll -PathType Leaf)) {
    throw "SpoutLibrary.dll must be deployed beside the Godot executable: $spoutDll"
}

$tempRoot = [System.IO.Path]::GetFullPath((Join-Path $customizations ".tmp"))
$project = [System.IO.Path]::GetFullPath((Join-Path $tempRoot "spout-lifecycle-smoke"))
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

$baselineIds = @(Get-MatchingGodotProcesses | Select-Object -ExpandProperty Id)
$leaked = @()
try {
    if (Test-Path -LiteralPath $project) {
        Remove-Item -LiteralPath $project -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $project | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "project.godot") -Destination $project
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "smoke.gd") -Destination $project

    $log = Join-Path $project "spout-lifecycle.log"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $godot --verbose --headless --path $project --script "res://smoke.gd" *> $log
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $output = Get-Content -LiteralPath $log -Raw -Encoding Unicode
    if ($exitCode -ne 0) {
        Write-Output $output
        throw "Spout lifecycle smoke failed with exit code $exitCode."
    }
    if ($output.IndexOf("SPOUT_LIFECYCLE_OK", [System.StringComparison]::Ordinal) -lt 0) {
        throw "Spout lifecycle smoke success marker is missing."
    }
    foreach ($forbidden in @(
        "SPOUT_LIFECYCLE_FAILED",
        "Leaked instance:",
        "Resource still in use:",
        "ObjectDB instance was leaked",
        "ObjectDB instances were leaked",
        "resources still in use at exit",
        "RID allocations of type"
    )) {
        if ($output.IndexOf($forbidden, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            Write-Output $output
            throw "Spout lifecycle smoke emitted forbidden diagnostic: $forbidden"
        }
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

Write-Output "Spout lifecycle smoke passed."
