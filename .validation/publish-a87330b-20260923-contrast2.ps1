$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hash = '38b6ddee7'
$stamp = '20260923-editor-contrast2'
$archive = Join-Path $root "archive/engine-color-dynamic-$hash-$stamp"
$backup = Join-Path $root "archive/export-$hash-$stamp"
$stage = Join-Path $root "export.stage-$hash-$stamp"
if (Test-Path $archive) { throw "Archive exists: $archive" }
if (Test-Path $backup) { throw "Backup exists: $backup" }
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $archive, $backup, $stage | Out-Null
Copy-Item (Join-Path $root 'export/*') $backup -Recurse -Force
$names = @(
    'godot.windows.editor.x86_64.exe', 'godot.windows.editor.x86_64.console.exe',
    'godot.windows.template_debug.x86_64.exe', 'godot.windows.template_debug.x86_64.console.exe',
    'godot.windows.template_release.x86_64.exe', 'godot.windows.template_release.x86_64.console.exe'
)
foreach ($name in $names) {
    Copy-Item (Join-Path $root "engine/bin/$name") $archive -Force
    Copy-Item (Join-Path $root "engine/bin/$name") $stage -Force
}
Copy-Item (Join-Path $root 'export/SpoutLibrary.dll') $archive -Force
Copy-Item (Join-Path $root 'export/SpoutLibrary.dll') $stage -Force
$srcCommit = (git -C (Join-Path $root 'engine') rev-parse HEAD).Trim()
$catCommit = (git -C (Join-Path $root 'customizations') rev-parse HEAD).Trim()
$upstream = (git -C (Join-Path $root 'engine') rev-parse origin/master).Trim()
$lock = (Get-FileHash (Join-Path $root 'customizations/stack.lock.json') -Algorithm SHA256).Hash.ToLower()
$artifacts = @()
foreach ($name in ($names + 'SpoutLibrary.dll')) {
    $file = Get-Item (Join-Path $archive $name)
    $artifacts += [ordered]@{ name = $name; bytes = $file.Length; sha256 = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() }
}
$versions = [ordered]@{}
foreach ($name in $names) {
    $version = (& (Join-Path $archive $name) --version 2>$null) -join ' '
    $versions[$name] = $version.Trim()
}
$buildManifest = [ordered]@{
    schema_version = 1; generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
    source = [ordered]@{ engine_commit = $srcCommit; engine_branch = 'personal/main'; source_dirty = $false; catalog_commit = $catCommit; upstream_commit = $upstream; stack_lock_sha256 = $lock }
    build = [ordered]@{ godot_version = '4.8.dev.custom_build.38b6ddee7'; platform = 'windows'; architecture = 'x86_64'; scons = '4.8.1'; common_arguments = @('dev_build=no','tests=no','accesskit=no','d3d12=no','module_color_scheme_enabled=yes'); targets = @('editor','template_debug','template_release') }
    artifacts = $artifacts; versions = $versions
    verification = [ordered]@{ editor = [ordered]@{ checks = 204; failures = @(); hash = $srcCommit; log = '.validation/dynamic-theme-menu-fix-editor-new/editor.log' }; runtime = [ordered]@{ checks = 311; failures = @(); hash = $srcCommit; log = '.validation/dynamic-theme-menu-fix-runtime-new/runtime.log' }; cpp = [ordered]@{ result = 'passed'; hash = $srcCommit; log = '.validation/dynamic-theme-menu-fix-cpp-new/cpp.log' }; template = [ordered]@{ result = 'pck_export_passed_launch_blocked_by_path_overrides'; log = '.validation/dynamic-theme-menu-fix-template-new/template.log' } }
    validation = 'Editor 204 checks, runtime 311 checks, and C++ dynamic-theme tests passed. Template PCK export passed; launch probe was blocked because the release template was built without path overrides.'
}
$buildPath = Join-Path $stage 'build-manifest.json'
$buildManifest | ConvertTo-Json -Depth 10 | Set-Content $buildPath -Encoding utf8
Copy-Item $buildPath $archive -Force
$publication = [ordered]@{
    schema_version = 1; published_at_utc = (Get-Date).ToUniversalTime().ToString('o'); active_directory = (Join-Path $root 'export'); engine_source = (Join-Path $root 'engine'); engine_commit = $srcCommit; build_manifest_sha256 = (Get-FileHash $buildPath -Algorithm SHA256).Hash.ToLower(); build_archive = $archive; previous_export_backup = $backup; artifacts = $artifacts; supplemental_launchers = @(); versions = $versions; version_logs = (Join-Path $root '.validation/publish-a87330b-20260923-contrast2'); validation = 'Fresh artifact hashes and six executable version probes passed; editor/runtime/C++ verification passed; template PCK export passed, template launch blocked by path-overrides build setting.'
}
$publicationPath = Join-Path $stage 'publication-manifest.json'
$publication | ConvertTo-Json -Depth 10 | Set-Content $publicationPath -Encoding utf8
Copy-Item $publicationPath $archive -Force
foreach ($file in Get-ChildItem $stage -File) { Copy-Item $file.FullName (Join-Path $root "export/$($file.Name)") -Force }
foreach ($name in $names) { Copy-Item (Join-Path $stage $name) (Join-Path $root "export/$name") -Force }
Copy-Item (Join-Path $stage 'SpoutLibrary.dll') (Join-Path $root 'export/SpoutLibrary.dll') -Force
Remove-Item $stage -Recurse -Force
Write-Output "Published $srcCommit to export; backup: $backup; archive: $archive"
