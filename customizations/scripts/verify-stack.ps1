[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$SkipBranchComparison,
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
    $config = Get-PatchStackConfig -StackPath $stack
    $lock = Assert-PatchStackCatalog -StackPath $stack -Config $config

    if (-not $SkipBranchComparison) {
        Assert-NoGitOperation -Repository $engine
        Assert-CleanGitWorktree -Repository $engine
        $branch = Get-GitBranch -Repository $engine
        if ($branch -ne [string]$lock.integration.branch) {
            Throw-PatchStackError -Message (
                "Lock expects integration branch '$($lock.integration.branch)', current branch is '$branch'."
            ) -ExitCode 1
        }
        $head = Resolve-GitCommit -Repository $engine -Reference "HEAD"
        if ($head -ne [string]$lock.integration.commit) {
            Throw-PatchStackError -Message (
                "Integration HEAD differs from the exported lock. Export patches again."
            ) -ExitCode 1
        }

        $baseCommit = [string]$lock.upstream.commit
        [void](Resolve-GitCommit -Repository $engine -Reference $baseCommit)
        $commitsResult = Invoke-GitChecked -Repository $engine -Arguments @(
            "rev-list", "--reverse", "--topo-order", "$baseCommit..$head"
        ) -Description "List integration commits"
        $commits = @($commitsResult.Lines | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_)
        })
        $entries = @($lock.patches)
        if ($commits.Count -ne $entries.Count) {
            Throw-PatchStackError -Message (
                "Integration branch contains $($commits.Count) personal commits, lock contains $($entries.Count)."
            ) -ExitCode 1
        }
        for ($index = 0; $index -lt $commits.Count; $index++) {
            if ($commits[$index] -ne [string]$entries[$index].sourceCommit) {
                Throw-PatchStackError -Message (
                    "Source commit mismatch at patch position $($index + 1). Export patches again."
                ) -ExitCode 1
            }
        }
    }

    if ($RunTests) {
        Invoke-PatchStackVerificationCommands -EnginePath $engine -Config $config -Lock $lock
    }

    Write-Output "Patch stack verification passed."
    Write-Output "Patches: $($lock.patchCount)"
    Write-Output "Base: $($lock.upstream.ref) ($($lock.upstream.commit))"
    Write-Output "Integration: $($lock.integration.branch) ($($lock.integration.commit))"
    exit 0
}
catch {
    [Console]::Error.WriteLine("Patch stack verification failed: $($_.Exception.Message)")
    exit 1
}
