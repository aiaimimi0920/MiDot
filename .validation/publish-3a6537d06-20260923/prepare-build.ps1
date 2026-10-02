$ErrorActionPreference = 'Stop'
$workspace = 'C:\Users\Public\nas_home\godot'
$engine = Join-Path $workspace 'engine'
$evidence = Join-Path $workspace '.validation\publish-3a6537d06-20260923'
$archive = Join-Path $workspace 'archive\engine-color-dynamic-3a6537d06-20260923'
$commit = '3a6537d06bca0cbcacfa8f70952ad9451deba199'
$version = '4.8.dev.custom_build.3a6537d06'
$validationPath = Join-Path $workspace 'customizations\topics\ui\dynamic-color-theme\validation-20260923.json'
$validation = Get-Content -LiteralPath $validationPath -Raw -Encoding UTF8 | ConvertFrom-Json

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-Record([string]$Path) {
    [ordered]@{ path = $Path; bytes = (Get-Item -LiteralPath $Path).Length; sha256 = (Get-Sha256 $Path) }
}

function Assert-Artifact([string]$Path, [object]$Record) {
    if ((Get-Item -LiteralPath $Path).Length -ne $Record.bytes -or (Get-Sha256 $Path) -ne $Record.sha256) {
        throw "Artifact changed since validation: $Path"
    }
}

if ([IO.File]::ReadAllText((Join-Path $evidence 'build-editor.exit-code.txt')).Trim() -ne '0') {
    throw 'The current regular editor build has not succeeded.'
}
$head = (rtk proxy git -C $engine rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $commit -or $validation.engine.commit -ne $commit) { throw 'Unexpected engine revision' }
$dirty = @(rtk proxy git -C $engine status --porcelain)
if ($LASTEXITCODE -ne 0 -or $dirty.Count) { throw 'Engine source is dirty' }
$catalog = (rtk proxy git -C (Join-Path $workspace 'customizations') rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the patch catalog' }
if (Test-Path -LiteralPath $archive) { throw "Build archive already exists: $archive" }
if ((Get-Item -LiteralPath (Split-Path -Parent $archive)).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'Build archive parent is a reparse point'
}

# The regular templates and their launchers retain the identities already tested.
foreach ($target in @('template_debug', 'template_release')) {
    $build = @($validation.builds | Where-Object name -eq $target)
    if ($build.Count -ne 1 -or $build[0].exit_code -ne 0) { throw "Missing validated build: $target" }
    foreach ($record in @($build[0].executable, $build[0].console)) {
        Assert-Artifact (Join-Path $workspace $record.path) $record
    }
}
$dll = @($validation.runtime_dependencies | Where-Object path -eq 'engine/bin/SpoutLibrary.dll')
if ($dll.Count -ne 1) { throw 'Missing validated Spout dependency' }
Assert-Artifact (Join-Path $workspace $dll[0].path) $dll[0]

New-Item -ItemType Directory -Path $archive | Out-Null
$names = @(
    'godot.windows.editor.x86_64.exe', 'godot.windows.editor.x86_64.console.exe',
    'godot.windows.template_debug.x86_64.exe', 'godot.windows.template_debug.x86_64.console.exe',
    'godot.windows.template_release.x86_64.exe', 'godot.windows.template_release.x86_64.console.exe',
    'SpoutLibrary.dll'
)
$artifacts = @()
foreach ($name in $names) {
    $source = Join-Path (Join-Path $engine 'bin') $name
    $record = [ordered]@{ name = $name; bytes = (Get-Item -LiteralPath $source).Length; sha256 = (Get-Sha256 $source) }
    Copy-Item -LiteralPath $source -Destination (Join-Path $archive $name)
    Assert-Artifact (Join-Path $archive $name) $record
    $artifacts += $record
}

$versions = [ordered]@{}
foreach ($name in $names | Where-Object { $_ -like '*.exe' }) {
    $stdout = Join-Path $evidence ($name + '.version.log')
    $stderr = Join-Path $evidence ($name + '.version.stderr.log')
    $arguments = @('proxy', ('"' + (Join-Path $archive $name) + '"'), '--version')
    $process = Start-Process -FilePath (Get-Command rtk.exe).Source -ArgumentList $arguments -WorkingDirectory $archive -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $process.Handle
    try {
        if (-not $process.WaitForExit(30000)) { throw "Version probe timed out: $name" }
        $process.Refresh()
        $actual = [IO.File]::ReadAllText($stdout).Trim()
        if ($process.ExitCode -ne 0 -or $actual -ne $version -or [IO.File]::ReadAllText($stderr).Trim()) {
            throw "Version probe failed: $name"
        }
        $versions[$name] = $actual
    } finally {
        Get-CimInstance Win32_Process -Filter "Name LIKE 'godot%.exe'" | Where-Object {
            $_.ExecutablePath -and $_.ExecutablePath.StartsWith($archive + '\', [StringComparison]::OrdinalIgnoreCase)
        } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
        $process.Dispose()
    }
}

$results = [ordered]@{}
foreach ($mode in @('Runtime', 'Editor')) {
    $output = Join-Path $evidence $mode.ToLowerInvariant()
    rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $workspace 'customizations\tests\verify-dynamic-color-theme.ps1') -GodotPath (Join-Path $archive 'godot.windows.editor.x86_64.exe') -Mode $mode -OutputPath $output
    if ($LASTEXITCODE -ne 0) { throw "Regular editor $mode verification failed" }
    $log = Join-Path $output ($mode.ToLowerInvariant() + '.log')
    $line = @(Get-Content -LiteralPath $log -Encoding UTF8 | Where-Object { $_ -match '^DYNAMIC_THEME_(RUNTIME|EDITOR) ' })
    if ($line.Count -ne 1) { throw "Missing $mode result" }
    $result = $line[0].Substring($line[0].IndexOf('{')) | ConvertFrom-Json
    $expectedChecks = if ($mode -eq 'Runtime') { 311 } else { 201 }
    if ($result.hash -ne $commit -or $result.checks -ne $expectedChecks -or @($result.failures).Count) {
        throw "Unexpected $mode result"
    }
    $results[$mode.ToLowerInvariant()] = [ordered]@{ result = $result; log = (Get-Record $log) }
}

$remaining = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'godot%.exe'" | Where-Object {
    $_.ExecutablePath -and $_.ExecutablePath.StartsWith($archive + '\', [StringComparison]::OrdinalIgnoreCase)
})
if ($remaining.Count) { throw 'Build verification left Godot processes running' }
foreach ($record in $artifacts) { Assert-Artifact (Join-Path $archive $record.name) $record }
$manifest = [ordered]@{
    schema_version = 1
    generated_at_utc = [DateTime]::UtcNow.ToString('o')
    source = [ordered]@{
        engine_commit = $commit; engine_branch = 'personal/main'; source_dirty = $false
        catalog_commit = $catalog; upstream_commit = $validation.engine.origin_master
        stack_lock_sha256 = (Get-Sha256 (Join-Path $workspace 'customizations\stack.lock.json'))
    }
    build = [ordered]@{
        godot_version = $version; platform = 'windows'; architecture = 'x86_64'; scons = '4.8.1'
        common_arguments = @('dev_build=no', 'tests=no', 'accesskit=no', 'd3d12=no', 'module_color_scheme_enabled=yes')
        targets = @('editor', 'template_debug', 'template_release')
        editor_arguments = @('platform=windows', 'target=editor', 'dev_build=no', 'tests=no', 'accesskit=no', 'd3d12=no', 'module_color_scheme_enabled=yes', '-j6')
    }
    artifacts = $artifacts
    versions = $versions
    logs = @((Get-Record (Join-Path $evidence 'build-editor.log')), (Get-Record (Join-Path $evidence 'build-editor.stderr.log')), (Get-Record (Join-Path $evidence 'build-editor.exit-code.txt')))
    verification = $results
    prior_validation_manifest = (Get-Record $validationPath)
    template_verification = [ordered]@{ debug = $validation.checks.template_debug; release = $validation.checks.template_release }
    validation = 'Regular editor build passed 311 runtime and 201 editor checks. Both standard templates retain the previously verified identities and 311-check PCK results. All six executable version probes passed.'
}
$manifestPath = Join-Path $archive 'build-manifest.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
Write-Output "BUILD_ARCHIVE $archive"
Write-Output "BUILD_MANIFEST_SHA256 $(Get-Sha256 $manifestPath)"
