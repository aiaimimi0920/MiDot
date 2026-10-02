[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GodotPath,
    [ValidateSet('Runtime', 'Editor', 'Benchmark', 'Cpp', 'Template')][string]$Mode = 'Runtime',
    [string]$EditorPath = '',
    [string]$OutputPath = '',
    [ValidateRange(10, 1800)][int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot 'dynamic_color_theme')).Path
$godot = (Resolve-Path -LiteralPath $GodotPath).Path
if (-not $OutputPath) {
    $OutputPath = Join-Path $project 'output'
}
[void](New-Item -ItemType Directory -Force -Path $OutputPath)
$output = (Resolve-Path -LiteralPath $OutputPath).Path
if ($Mode -eq 'Template') {
    if (-not $EditorPath) {
        throw 'Template verification requires -EditorPath to export its PCK.'
    }
    $editor = (Resolve-Path -LiteralPath $EditorPath).Path
    $sourceGodot = $godot
    $godot = Join-Path $output 'godot.dynamic-theme-template.exe'
    Copy-Item -LiteralPath $sourceGodot -Destination $godot -Force
    $spout = Join-Path (Split-Path -Parent $sourceGodot) 'SpoutLibrary.dll'
    if (Test-Path -LiteralPath $spout) {
        Copy-Item -LiteralPath $spout -Destination $output -Force
    }
    $pack = [IO.Path]::ChangeExtension($godot, '.pck')
}
$stdout = Join-Path $output ($Mode.ToLowerInvariant() + '.log')
$stderr = Join-Path $output ($Mode.ToLowerInvariant() + '.stderr.log')
$arguments = @('--path', $project)
switch ($Mode) {
    'Runtime' { $arguments += @('--position', '12000,12000', '--rendering-method', 'gl_compatibility', '--script', 'runtime_smoke.gd') }
    'Editor' { $arguments += @('--editor', '--language', 'en', '--rendering-method', 'gl_compatibility', '--', '--theme-editor-smoke') }
    'Benchmark' { $arguments += @('--headless', '--script', 'benchmark.gd') }
    'Cpp' { $arguments += @('--headless', '--test', '--test-case=*[ColorScheme]*,*[Theme]*,*Dynamic theme*,*Static built-in*,*StyleBox*') }
    'Template' { $arguments = @('--position', '12000,12000', '--rendering-method', 'gl_compatibility') }
}

function Get-VerificationProcesses {
    $matchProject = $project.Replace('/', '\').ToLowerInvariant()
    $matchGodot = $godot.Replace('/', '\').ToLowerInvariant()
    @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^godot.*\.exe$' -and
        (([string]$_.CommandLine).Replace('/', '\').ToLowerInvariant().Contains($matchProject) -or
            ([string]$_.ExecutablePath).Replace('/', '\').ToLowerInvariant() -eq $matchGodot)
    })
}

$existingIds = @(Get-VerificationProcesses | ForEach-Object { $_.ProcessId })
$process = $null
$exitCode = 1
try {
    if ($Mode -eq 'Template') {
        $exportOut = Join-Path $output 'export.log'
        $exportErr = Join-Path $output 'export.stderr.log'
        $exportArguments = @('proxy', $editor, '--headless', '--path', $project, '--export-pack', 'TemplateSmoke', $pack)
        $exportArguments = @($exportArguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' })
        $process = Start-Process -FilePath (Get-Command rtk.exe).Source -ArgumentList $exportArguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $exportOut -RedirectStandardError $exportErr
        $null = $process.Handle
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            throw "Template PCK export timed out. See $exportOut and $exportErr."
        }
        $process.WaitForExit()
        $exportLog = [IO.File]::ReadAllText($exportOut) + [IO.File]::ReadAllText($exportErr)
        if ($process.ExitCode -ne 0 -or $exportLog -match 'SCRIPT ERROR:|(?m)^ERROR:' -or -not (Test-Path -LiteralPath $pack)) {
            throw "Template PCK export failed (exit $($process.ExitCode)). See $exportOut and $exportErr."
        }
    }
    $quotedArguments = @(@('proxy', $godot) + $arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' })
    $process = Start-Process -FilePath (Get-Command rtk.exe).Source -ArgumentList $quotedArguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    # Retain the native handle so Windows PowerShell 5.1 can read ExitCode after exit.
    $null = $process.Handle
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        throw "Dynamic theme $Mode verification timed out after $TimeoutSeconds seconds. See $stdout and $stderr."
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $log = [IO.File]::ReadAllText($stdout) + [IO.File]::ReadAllText($stderr)
    if ($exitCode -ne 0 -or $log -match 'SCRIPT ERROR:|(?m)^ERROR:') {
        throw "Dynamic theme $Mode verification failed (exit $exitCode). See $stdout and $stderr."
    }
    $marker = switch ($Mode) {
        'Runtime' { 'DYNAMIC_THEME_RUNTIME' }
        'Editor' { 'DYNAMIC_THEME_EDITOR' }
        'Benchmark' { 'BENCH_TEXTURE' }
        'Cpp' { '\[doctest\] Status: SUCCESS!' }
        'Template' { 'DYNAMIC_THEME_RUNTIME' }
    }
    if ($log -notmatch $marker -or $log -match 'test cases:\s+0\s+\|') {
        throw "Dynamic theme $Mode verification produced no completion evidence. See $stdout and $stderr."
    }
    Write-Output "$Mode passed. Evidence: $stdout"
}
finally {
    # Only terminate processes created for this dedicated verification project.
    Get-VerificationProcesses | Where-Object { $existingIds -notcontains $_.ProcessId } | ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    $cleanup = Join-Path $env:USERPROFILE '.codex-gd\skills\godot-local-toolchain\scripts\cleanup-godot-test-processes.ps1'
    if (Test-Path -LiteralPath $cleanup) {
        & $cleanup -ProjectPaths $project
    }
    $remaining = @(Get-VerificationProcesses | Where-Object { $existingIds -notcontains $_.ProcessId })
    if ($remaining.Count -gt 0) {
        throw "Dynamic theme verification left processes running: $($remaining.ProcessId -join ', ')."
    }
}
