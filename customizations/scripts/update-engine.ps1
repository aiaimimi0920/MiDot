[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$RunTests,
    [switch]$Finalize
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
    Assert-EngineGitRepository -Repository $engine
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
    $mirrorRef = "refs/heads/" + [string]$config.upstream.branch
    $mirror = Resolve-GitCommit -Repository $engine -Reference $mirrorRef
    $statePath = Join-Path (Get-GitDirectory -Repository $engine) "patch-stack-update.json"
    $configHash = Get-FileSha256 -Path (Join-Path $stack "stack.json")
    Assert-GitBranchNotCheckedOut -Repository $engine -Reference $mirrorRef

    if ($Finalize) {
        if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
            Throw-PatchStackError -Message "No pending patch-stack update exists to finalize." -ExitCode 3
        }
        $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($state.schemaVersion -ne 1 -or $state.branch -ne $branch -or
            $state.catalog -ne $stack -or $state.configSha256 -ne $configHash) {
            Throw-PatchStackError -Message "Pending update belongs to a different catalog, configuration, or branch." -ExitCode 3
        }
        # A retry cannot silently drop verification requested by the original update.
        $RunTests = $RunTests -or [bool]$state.runTests
        $backupRef = [string]$state.backupRef
        if ((Resolve-GitCommit -Repository $engine -Reference $backupRef) -ne [string]$state.originalHead) {
            Throw-PatchStackError -Message "Pending update safety ref changed; inspect it before finalizing." -ExitCode 3
        }
        $upstreamCommit = Resolve-GitCommit -Repository $engine -Reference ([string]$state.target)
        if ($mirror -ne [string]$state.originalBase -and $mirror -ne $upstreamCommit) {
            Throw-PatchStackError -Message "Upstream mirror changed outside the pending update." -ExitCode 3
        }
        $originalCatalog = $lock.integration.commit -eq $state.originalHead -and
            $lock.upstream.commit -eq $state.originalBase
        $completedCatalog = $lock.integration.commit -eq $head -and
            $lock.upstream.commit -eq $upstreamCommit
        if (-not $originalCatalog -and -not $completedCatalog) {
            Throw-PatchStackError -Message "Catalog changed outside the pending update." -ExitCode 3
        }
        $ancestor = Invoke-GitCapture -Repository $engine -Arguments @(
            "merge-base", "--is-ancestor", $upstreamCommit, $head
        )
        if ($ancestor.ExitCode -ne 0) {
            Throw-PatchStackError -Message "Finish the rebase onto the recorded target before finalizing." -ExitCode 3
        }
        # Export uses the configured remote-tracking ref. Refuse a later fetch
        # rather than silently export a different base than the resumed update.
        if ((Resolve-GitCommit -Repository $engine -Reference (Get-PatchStackShortRef -Config $config)) -ne $upstreamCommit) {
            Throw-PatchStackError -Message "Upstream tracking ref changed since this update; inspect the pending state." -ExitCode 3
        }
    }
    else {
        if (Test-Path -LiteralPath $statePath) {
            Throw-PatchStackError -Message (
                "A pending update exists. Complete its rebase and rerun with -Finalize. " +
                "After an explicit rebase --abort, inspect and remove the pending state file: $statePath"
            ) -ExitCode 3
        }
        if ($head -ne [string]$lock.integration.commit -or $mirror -ne [string]$lock.upstream.commit) {
            Throw-PatchStackError -Message "Integration branch or upstream mirror differs from the exported catalog." -ExitCode 3
        }
        $backupRef = "refs/patch-stack/backups/" + [guid]::NewGuid().ToString("N")
        [void](Invoke-GitChecked -Repository $engine -Arguments @(
            "update-ref", $backupRef, $head
        ) -Description "Create update safety ref")
        Write-Output "Created safety ref: $backupRef"
        $remote = [string]$config.upstream.remote
        $upstreamBranch = [string]$config.upstream.branch
        [void](Invoke-GitChecked -Repository $engine -Arguments @(
            "fetch", $remote, $upstreamBranch
        ) -Description "Fetch engine upstream")
        $upstreamRef = Get-PatchStackShortRef -Config $config
        $upstreamCommit = Resolve-GitCommit -Repository $engine -Reference $upstreamRef
        $mergeBase = Invoke-GitCapture -Repository $engine -Arguments @("merge-base", "HEAD", $upstreamCommit)
        if ($mergeBase.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($mergeBase.Text)) {
            Throw-PatchStackError -Message "No merge base is available. Deepen the shallow repository before rebasing."
        }
        $state = [ordered]@{
            schemaVersion = 1
            catalog = $stack
            configSha256 = $configHash
            branch = $branch
            originalHead = $head
            originalBase = $mirror
            target = $upstreamCommit
            backupRef = $backupRef
            runTests = [bool]$RunTests
        }
        Write-Utf8NoBom -Path $statePath -Content (($state | ConvertTo-Json) + "`n")
        Write-Output "Rebasing $branch onto $upstreamRef ($upstreamCommit)..."
        $rebaseResult = Invoke-GitCapture -Repository $engine -Arguments @(
            "rebase", $upstreamCommit
        )
        if (-not [string]::IsNullOrWhiteSpace($rebaseResult.Text)) { Write-Output $rebaseResult.Text }
        if ($rebaseResult.ExitCode -ne 0) {
            [Console]::Error.WriteLine(
                "Rebase stopped. Resolve and run git rebase --continue, then rerun this script with -Finalize. " +
                "Or explicitly abort the rebase; pending state remains at $statePath. Safety ref: $backupRef"
            )
            exit 5
        }
    }
    Assert-GitBranchNotCheckedOut -Repository $engine -Reference $mirrorRef
    [void](Invoke-GitChecked -Repository $engine -Arguments @(
        "update-ref", $mirrorRef, $upstreamCommit, $mirror
    ) -Description "Advance pristine mirror without overwriting unrelated changes")

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

    Remove-Item -LiteralPath $statePath
    Write-Output "Godot upstream update completed with rebase."
    Write-Output "Safety ref: $backupRef"
    exit 0
}
catch {
    $exitCode = Get-PatchStackExitCode -ErrorRecord $_ -Default 2
    [Console]::Error.WriteLine("Engine update failed: $($_.Exception.Message)")
    exit $exitCode
}

