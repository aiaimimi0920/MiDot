$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$engine = "C:/Users/Public/nas_home/godot/export"
$editor = Join-Path $engine "godot.windows.editor.x86_64.exe"
$project = Join-Path $root "project"
$release = Join-Path $root "release-package"
$debug = Join-Path $root "debug-package"
New-Item -ItemType Directory -Path $release, $debug -Force | Out-Null
foreach ($directory in @($release, $debug)) {
    Copy-Item -LiteralPath (Join-Path $engine "SpoutLibrary.dll") -Destination $directory -Force
}
Copy-Item -LiteralPath "C:/Users/Public/nas_home/godot/customizations/topics/media/spout/verification/smoke.gd" -Destination (Join-Path $project "spout_smoke.gd") -Force

function Invoke-Probe([string]$Executable, [string]$Label, [string[]]$Arguments, [string]$WorkingDirectory, [string]$Marker) {
    $output = Join-Path $root $Label
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    $stdout = Join-Path $output "stdout.log"
    $stderr = Join-Path $output "stderr.log"
    $quoted = @($Arguments | ForEach-Object { '"' + $_ + '"' })
    $process = Start-Process -FilePath $Executable -WorkingDirectory $WorkingDirectory -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $process.Handle
    try {
        if (-not $process.WaitForExit(180000)) { throw "Probe timeout: $Label" }
        $process.Refresh()
        $log = [IO.File]::ReadAllText($stdout) + [IO.File]::ReadAllText($stderr)
        $issues = @($log -split "`n" | Where-Object { $_ -match "ERROR:|WARNING:|SCRIPT ERROR:|Parse Error|Leaked instance|still in use at exit|ObjectDB.*leaked" })
        Write-Output "$Label EXIT=$($process.ExitCode) ISSUES=$($issues.Count)"
        if ($process.ExitCode -ne 0 -or $issues.Count -or ($Marker -and $log -notmatch $Marker)) {
            $issues | Select-Object -First 10 | Write-Output
            $log -split "`n" | Where-Object { $_ -match "CHECK_FAILED|NON_NPR_CHECKS" } | Write-Output
            throw "Probe failed: $Label; see $output"
        }
        $log -split "`n" | Where-Object { $_ -match "NON_NPR_CHECKS|SPOUT_LIFECYCLE_OK" } | Write-Output
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.Dispose()
    }
}

Invoke-Probe $editor "import" @("--headless", "--path", $project, "--editor", "--import", "--quit") $project ""
Invoke-Probe $editor "editor" @("--headless", "--path", $project, "--audio-driver", "Dummy", "--", (Join-Path $root "editor")) $project "NON_NPR_CHECKS .* FAILURES 0"
Invoke-Probe $editor "spout-lifecycle" @("--headless", "--path", $project, "--script", "res://spout_smoke.gd") $project "SPOUT_LIFECYCLE_OK"
Invoke-Probe $editor "export-release" @("--headless", "--path", $project, "--export-release", "Engine Feature Probe", (Join-Path $release "EngineFeatureProbe.exe")) $project ""
Invoke-Probe (Join-Path $release "EngineFeatureProbe.exe") "release" @("--headless", "--audio-driver", "Dummy", "--", (Join-Path $root "release")) $release "NON_NPR_CHECKS .* FAILURES 0"
Invoke-Probe $editor "export-debug" @("--headless", "--path", $project, "--export-debug", "Engine Feature Probe", (Join-Path $debug "EngineFeatureProbe.exe")) $project ""
Invoke-Probe (Join-Path $debug "EngineFeatureProbe.exe") "debug" @("--headless", "--audio-driver", "Dummy", "--", (Join-Path $root "debug")) $debug "NON_NPR_CHECKS .* FAILURES 0"
Write-Output "NON_NPR_BINARY_VERIFICATION_OK"
