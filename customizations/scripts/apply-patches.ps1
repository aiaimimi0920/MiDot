[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$AllowBaseMismatch,
    [switch]$NoThreeWay
)

if ([string]::IsNullOrWhiteSpace($StackPath)) {
    $StackPath = Join-Path $PSScriptRoot ".."
}

. (Join-Path $PSScriptRoot "lib\PatchStack.Common.ps1")

$temporaryRoot = $null

try {
    Assert-GitAvailable
    $engine = Resolve-PatchStackPath -Path $EnginePath -MustExist
    $stack = Resolve-PatchStackPath -Path $StackPath -MustExist
    Assert-EngineGitRepository -Repository $engine
    Assert-GitRepository -Repository $stack
    Assert-NoGitOperation -Repository $engine
    Assert-CleanGitWorktree -Repository $engine

    $config = Get-PatchStackConfig -StackPath $stack
    $lock = Assert-PatchStackCatalog -StackPath $stack -Config $config
    $head = Resolve-GitCommit -Repository $engine -Reference "HEAD"
    $expectedBase = [string]$lock.upstream.commit
    if ($head -ne $expectedBase -and -not $AllowBaseMismatch) {
        Throw-PatchStackError -Message (
            "Target HEAD is $head, but the patch stack was exported from $expectedBase. " +
            "Use a branch at the locked base, or explicitly pass -AllowBaseMismatch."
        ) -ExitCode 3
    }
    if ($head -ne $expectedBase) {
        Write-Warning (
            "Applying to a different upstream commit. Git may stop for semantic conflict resolution."
        )
    }

    $series = @(Get-PatchStackSeries -StackPath $stack)
    if ($series.Count -eq 0) {
        Write-Output "Patch stack is empty; nothing to apply."
        exit 0
    }

    $temporaryRoot = Join-Path $stack ".tmp"
    [void](New-Item -ItemType Directory -Path $temporaryRoot -Force)
    $mailbox = Join-Path $temporaryRoot ("apply-" + [guid]::NewGuid().ToString("N") + ".mbox")
    $output = [System.IO.File]::Open(
        $mailbox,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None
    )
    try {
        foreach ($relative in $series) {
            $patchPath = [System.IO.Path]::GetFullPath((Join-Path $stack $relative))
            Assert-PatchStackChildPath -Parent $stack -Child $patchPath -Description "Patch path"
            $input = [System.IO.File]::OpenRead($patchPath)
            try {
                $input.CopyTo($output)
            }
            finally {
                $input.Dispose()
            }
            $output.WriteByte(10)
        }
    }
    finally {
        $output.Dispose()
    }

    $arguments = @("-C", $engine, "am")
    if (-not $NoThreeWay) {
        $arguments += "--3way"
    }
    $arguments += $mailbox
    & git @arguments
    $applyExitCode = $LASTEXITCODE
    Remove-Item -LiteralPath $mailbox -Force -ErrorAction SilentlyContinue

    if ($applyExitCode -ne 0) {
        [Console]::Error.WriteLine(
            "Patch application stopped. Resolve and run 'git am --continue', " +
            "or run 'git am --abort'. No patch was skipped automatically."
        )
        exit 4
    }

    Write-Output "Applied $($series.Count) patch(es) successfully."
    exit 0
}
catch {
    $exitCode = Get-PatchStackExitCode -ErrorRecord $_ -Default 2
    [Console]::Error.WriteLine("Patch application failed: $($_.Exception.Message)")
    exit $exitCode
}
