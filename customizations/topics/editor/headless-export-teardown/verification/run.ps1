[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$GodotExecutable,
    [string]$TemplateExecutable
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
if ([string]::IsNullOrWhiteSpace($TemplateExecutable)) {
    $TemplateExecutable = Join-Path $customizations "..\export\godot.windows.template_release.x86_64.exe"
}
$godot = [System.IO.Path]::GetFullPath($GodotExecutable)
$template = [System.IO.Path]::GetFullPath($TemplateExecutable)
if (-not (Test-Path -LiteralPath $godot -PathType Leaf)) {
    throw "Godot editor executable not found: $godot"
}
if (-not (Test-Path -LiteralPath $template -PathType Leaf)) {
    throw "Godot release template not found: $template"
}

$tempRoot = [System.IO.Path]::GetFullPath((Join-Path $customizations ".tmp"))
$project = [System.IO.Path]::GetFullPath((Join-Path $tempRoot "headless-export-teardown"))
$expectedPrefix = $tempRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $project.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to manage a smoke project outside the customization temp root: $project"
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
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

    Write-Utf8NoBom -Path (Join-Path $project "project.godot") -Content @'
[application]
config/name="Headless Export Teardown Smoke"
run/main_scene="res://main.tscn"

[rendering]
renderer/rendering_method="gl_compatibility"
renderer/rendering_method.mobile="gl_compatibility"
'@
    Write-Utf8NoBom -Path (Join-Path $project "main.tscn") -Content @'
[gd_scene format=3]

[node name="Main" type="Node2D"]
'@
    $templateConfigPath = $template.Replace('\', '/')
    Write-Utf8NoBom -Path (Join-Path $project "export_presets.cfg") -Content @"
[preset.0]

name="windows-release-64"
platform="Windows Desktop"
runnable=true
dedicated_server=false
custom_features=""
export_filter="all_resources"
include_filter=""
exclude_filter=""
export_path="missing-parent/game.exe"
script_export_mode=2

[preset.0.options]

custom_template/release="$templateConfigPath"
binary_format/architecture="x86_64"
texture_format/s3tc_bptc=true
texture_format/etc2_astc=false
"@

    $log = Join-Path $project "failed-export.log"
    $output = Join-Path $project "missing-parent\game.exe"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $godot --headless --path $project --export-release "windows-release-64" $output *> $log
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $content = Get-Content -LiteralPath $log -Raw
    if ($exitCode -ne 1) {
        Get-Content -LiteralPath $log
        throw "Failed-export smoke expected exit code 1, got $exitCode."
    }
    if ($content.IndexOf('Parameter "singleton" is null', [System.StringComparison]::Ordinal) -ge 0) {
        throw "Editor singleton null diagnostic reappeared during failed-export teardown."
    }
    if ($content.IndexOf('Project export for preset "windows-release-64" failed.', [System.StringComparison]::Ordinal) -lt 0) {
        Get-Content -LiteralPath $log
        throw "Expected failed-export diagnostic was not emitted."
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

Write-Output "Headless failed-export teardown smoke passed."
