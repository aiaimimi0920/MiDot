[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$AllowDirty,
    [switch]$RequireIntegrationBranch
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
    if (-not $AllowDirty) {
        Assert-CleanGitWorktree -Repository $engine
    }

    $config = Get-PatchStackConfig -StackPath $stack
    $remoteUrl = Assert-PatchStackRemote -EnginePath $engine -Config $config
    $branch = Get-GitBranch -Repository $engine
    if ($RequireIntegrationBranch -and $branch -ne [string]$config.integrationBranch) {
        Throw-PatchStackError -Message (
            "Expected integration branch '$($config.integrationBranch)', current branch is '$branch'."
        ) -ExitCode 3
    }

    $head = Resolve-GitCommit -Repository $engine -Reference "HEAD"
    $upstreamRef = Get-PatchStackShortRef -Config $config
    $upstreamCommit = Resolve-GitCommit -Repository $engine -Reference $upstreamRef
    $shallow = (Invoke-GitChecked -Repository $engine -Arguments @(
        "rev-parse", "--is-shallow-repository"
    ) -Description "Read shallow repository state").Text

    Write-Output "Patch-stack preflight passed."
    Write-Output "Engine: $engine"
    Write-Output "Stack: $stack"
    Write-Output "Branch: $branch"
    Write-Output "HEAD: $head"
    Write-Output "Upstream: $upstreamRef ($upstreamCommit)"
    Write-Output "Remote URL: $remoteUrl"
    Write-Output "Shallow: $shallow"
    exit 0
}
catch {
    $exitCode = Get-PatchStackExitCode -ErrorRecord $_ -Default 2
    [Console]::Error.WriteLine("Preflight failed: $($_.Exception.Message)")
    exit $exitCode
}
