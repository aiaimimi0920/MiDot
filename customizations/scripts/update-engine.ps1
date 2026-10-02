[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$RunTests
)

if ([string]::IsNullOrWhiteSpace($EnginePath)) {
    $EnginePath = Join-Path $PSScriptRoot "..\..\engine"
}
if ([string]::IsNullOrWhiteSpace($StackPath)) {
    $StackPath = Join-Path $PSScriptRoot ".."
}

. (Join-Path $PSScriptRoot "lib\PatchStack.Common.ps1")

try {
    Assert-GitAvailable
    $engine = Resolve-PatchStackPath -Path $EnginePath -MustExist
    $stack = Resolve-PatchStackPath -Path $StackPath -MustExist
    Assert-GitRepository -Repository $engine
    Assert-GitRepository -Repository $stack
    Assert-NoGitOperation -Repository $engine
    Assert-CleanGitWorktree -Repository $engine

    $config = Get-PatchStackConfig -StackPath $stack
    $lock = Assert-PatchStackCatalog -StackPath $stack -Config $config
    $branch = Get-GitBranch -Repository $engine
    if ($branch -ne [string]$config.integrationBranch) {
        Throw-PatchStackError -Message (
            "Update requires integration branch '$($config.integrationBranch)', current branch is '$branch'."
        ) -ExitCode 3
    }

    $head = Resolve-GitCommit -Repository $engine -Reference "HEAD"
    if ($head -ne [string]$lock.integration.commit) {
        Throw-PatchStackError -Message (
            "Integration branch differs from the exported patch catalog. " +
            "Export and verify the current stack before updating upstream."
        ) -ExitCode 3
    }

    $timestamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $backupRef = "refs/patch-stack/backups/$timestamp-$($head.Substring(0, 12))"
    $backupResult = Invoke-GitCapture -Repository $engine -Arguments @(
        "update-ref", $backupRef, $head
    )
    if ($backupResult.ExitCode -ne 0) {
        Throw-PatchStackError -Message "Could not create safety ref: $($backupResult.Text)"
    }
    Write-Output "Created safety ref: $backupRef"

    $remote = [string]$config.upstream.remote
    $upstreamBranch = [string]$config.upstream.branch
    Write-Output "Fetching $remote $upstreamBranch..."
    & git -C $engine fetch $remote $upstreamBranch
    $fetchExitCode = $LASTEXITCODE
    if ($fetchExitCode -ne 0) {
        Throw-PatchStackError -Message "git fetch failed with exit code $fetchExitCode."
    }

    $upstreamRef = Get-PatchStackShortRef -Config $config
    $upstreamCommit = Resolve-GitCommit -Repository $engine -Reference $upstreamRef
    $mergeBase = Invoke-GitCapture -Repository $engine -Arguments @(
        "merge-base", "HEAD", $upstreamCommit
    )
    if ($mergeBase.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($mergeBase.Text)) {
        Throw-PatchStackError -Message (
            "No merge base is available for HEAD and $upstreamRef. " +
            "This shallow repository must be deepened before rebasing."
        )
    }

    Write-Output "Rebasing $branch onto $upstreamRef ($upstreamCommit)..."
    & git -C $engine rebase $upstreamRef
    $rebaseExitCode = $LASTEXITCODE
    if ($rebaseExitCode -ne 0) {
        [Console]::Error.WriteLine(
            "Rebase stopped. Resolve the current topic commit and run 'git rebase --continue', " +
            "or run 'git rebase --abort'. The safety ref is $backupRef."
        )
        exit 5
    }

    $powerShell = Join-Path $PSHOME "powershell.exe"
    if (-not (Test-Path -LiteralPath $powerShell -PathType Leaf)) {
        $powerShell = (Get-Process -Id $PID).Path
    }
    $exportScript = Join-Path $PSScriptRoot "export-patches.ps1"
    & $powerShell -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
        -File $exportScript -EnginePath $engine -StackPath $stack -Replace
    $exportExitCode = $LASTEXITCODE
    if ($exportExitCode -ne 0) {
        Throw-PatchStackError -Message (
            "Rebase succeeded, but patch export failed with exit code $exportExitCode. " +
            "The safety ref is $backupRef."
        )
    }

    $verifyScript = Join-Path $PSScriptRoot "verify-stack.ps1"
    $verifyArguments = @(
        "-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
        "-File", $verifyScript, "-EnginePath", $engine, "-StackPath", $stack
    )
    if ($RunTests) {
        $verifyArguments += "-RunTests"
    }
    & $powerShell @verifyArguments
    $verifyExitCode = $LASTEXITCODE
    if ($verifyExitCode -ne 0) {
        Throw-PatchStackError -Message (
            "Rebase and export succeeded, but verification failed with exit code $verifyExitCode. " +
            "The safety ref is $backupRef."
        ) -ExitCode 1
    }

    Write-Output "Godot upstream update completed with rebase."
    Write-Output "Safety ref: $backupRef"
    exit 0
}
catch {
    $exitCode = Get-PatchStackExitCode -ErrorRecord $_ -Default 2
    [Console]::Error.WriteLine("Engine update failed: $($_.Exception.Message)")
    exit $exitCode
}
