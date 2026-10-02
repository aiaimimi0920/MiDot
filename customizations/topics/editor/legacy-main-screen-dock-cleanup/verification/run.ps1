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
$project = [System.IO.Path]::GetFullPath((Join-Path $tempRoot "legacy-main-screen-dock-cleanup"))
$expectedPrefix = $tempRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $project.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to manage a smoke project outside the customization temp root: $project"
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
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
    $addon = Join-Path $project "addons\legacy_main_screen"
    New-Item -ItemType Directory -Force -Path $addon | Out-Null

    Write-Utf8NoBom -Path (Join-Path $project "project.godot") -Content @'
[application]
config/name="Legacy Main Screen Dock Cleanup Smoke"

[editor_plugins]
enabled=PackedStringArray("res://addons/legacy_main_screen/plugin.cfg")

[rendering]
renderer/rendering_method="gl_compatibility"
renderer/rendering_method.mobile="gl_compatibility"
'@
    Write-Utf8NoBom -Path (Join-Path $addon "plugin.cfg") -Content @'
[plugin]
name="Legacy Main Screen Smoke"
description="Exercises deprecated EditorPlugin main-screen compatibility."
author="Godot customization verification"
version="1.0"
script="plugin.gd"
'@
    Write-Utf8NoBom -Path (Join-Path $addon "plugin.gd") -Content @'
@tool
extends EditorPlugin

var main_view: Control

func _enter_tree() -> void:
    main_view = Control.new()
    main_view.name = "LegacyMainScreenSmoke"
    get_editor_interface().get_editor_main_screen().add_child(main_view)

func _exit_tree() -> void:
    if is_instance_valid(main_view):
        main_view.queue_free()

func _has_main_screen() -> bool:
    return true

func _get_plugin_name() -> String:
    return "Legacy Smoke"

func _make_visible(visible: bool) -> void:
    if is_instance_valid(main_view):
        main_view.visible = visible
'@

    $log = Join-Path $project "editor.log"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $godot --headless --verbose --editor --path $project --quit *> $log
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $content = Get-Content -LiteralPath $log -Raw
    if ($exitCode -ne 0) {
        Get-Content -LiteralPath $log
        throw "Legacy main-screen plugin smoke expected exit code 0, got $exitCode."
    }
    foreach ($unexpected in @(
        "Leaked instance: EditorDock",
        "Leaked instance: CompressedTexture2D",
        "Leaked instance: DummyTexture",
        "ObjectDB instances leaked at exit",
        "Cannot get path of node as it is not in a scene tree"
    )) {
        if ($content.IndexOf($unexpected, [System.StringComparison]::Ordinal) -ge 0) {
            Get-Content -LiteralPath $log
            throw "Legacy main-screen plugin shutdown emitted forbidden diagnostic: $unexpected"
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

Write-Output "Legacy main-screen dock cleanup smoke passed."
