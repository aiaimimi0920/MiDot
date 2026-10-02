[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [string]$UpstreamUrl = "https://github.com/godotengine/godot.git"
)

if ([string]::IsNullOrWhiteSpace($EnginePath)) {
    $EnginePath = Join-Path $PSScriptRoot "..\..\engine"
}
if ([string]::IsNullOrWhiteSpace($StackPath)) {
    $StackPath = Join-Path $PSScriptRoot ".."
}

. (Join-Path $PSScriptRoot "lib\PatchStack.Common.ps1")
$staging = $null

function Assert-InitializedEngine {
    param([string]$Repository, [string]$Catalog, [object]$Config, [object]$Lock)

    Assert-EngineGitRepository -Repository $Repository
    Assert-NoGitOperation -Repository $Repository
    Assert-CleanGitWorktree -Repository $Repository
    $remoteUrl = (Invoke-GitChecked -Repository $Repository -Arguments @(
        "remote", "get-url", [string]$Config.upstream.remote
    ) -Description "Read engine upstream URL").Text
    if ($remoteUrl -ne $UpstreamUrl) {
        Throw-PatchStackError -Message "Existing engine upstream URL differs: $remoteUrl" -ExitCode 3
    }
    $base = Resolve-GitCommit -Repository $Repository -Reference (
        "refs/heads/" + [string]$Config.upstream.branch
    )
    if ($base -ne [string]$Lock.upstream.commit) {
        Throw-PatchStackError -Message "Existing engine upstream mirror differs from the locked base." -ExitCode 3
    }
    $powerShell = (Get-Process -Id $PID).Path
    & $powerShell -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
        -File (Join-Path $PSScriptRoot "verify-stack.ps1") `
        -EnginePath $Repository -StackPath $Catalog
    if ($LASTEXITCODE -ne 0) {
        Throw-PatchStackError -Message "Initialized engine failed strict patch-stack verification." -ExitCode 1
    }
}

try {
    Assert-GitAvailable
    $engine = Resolve-PatchStackPath -Path $EnginePath
    $stack = Resolve-PatchStackPath -Path $StackPath -MustExist
    Assert-GitRepository -Repository $stack
    $config = Get-PatchStackConfig -StackPath $stack
    $lock = Assert-PatchStackCatalog -StackPath $stack -Config $config
    if (Test-Path -LiteralPath $engine) {
        Assert-InitializedEngine -Repository $engine -Catalog $stack -Config $config -Lock $lock
        Write-Output "Engine already initialized; no files or refs changed: $engine"
        exit 0
    }
    if ([int]$lock.patchCount -gt 0 -and
        $lock.PSObject.Properties.Name -notcontains "historyBundle") {
        Throw-PatchStackError -Message (
            "This catalog has no exact-history bundle. Export it with the current export-patches.ps1 first."
        )
    }

    $parent = Split-Path -Parent $engine
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        Throw-PatchStackError -Message "Engine parent directory does not exist: $parent"
    }
    $staging = Join-Path $parent (
        (Split-Path -Leaf $engine) + ".initialize-" + [guid]::NewGuid().ToString("N")
    )
    Assert-PatchStackChildPath -Parent $parent -Child $staging
    Assert-PatchStackChildPath -Parent $parent -Child $engine
    [void](Invoke-GitChecked -Repository $parent -Arguments @(
        "init", "-b", "bootstrap", $staging
    ) -Description "Create independent engine repository")
    [void](Invoke-GitChecked -Repository $staging -Arguments @(
        "config", "core.longpaths", "true"
    ) -Description "Enable Windows long paths")
    [void](Invoke-GitChecked -Repository $staging -Arguments @(
        "config", "core.autocrlf", "false"
    ) -Description "Keep upstream-controlled line endings")
    $remote = [string]$config.upstream.remote
    $upstreamBranch = [string]$config.upstream.branch
    $personalBranch = [string]$config.integrationBranch
    [void](Invoke-GitChecked -Repository $staging -Arguments @(
        "remote", "add", $remote, $UpstreamUrl
    ) -Description "Configure official engine upstream")
    Write-Output "Fetching locked upstream base $($lock.upstream.commit) from $UpstreamUrl..."
    [void](Invoke-GitChecked -Repository $staging -Arguments @(
        "fetch", "--depth=1", "--no-tags", $remote,
        "$($lock.upstream.commit):refs/remotes/$remote/$upstreamBranch"
    ) -Description "Fetch locked official base")
    [void](Invoke-GitChecked -Repository $staging -Arguments @(
        "checkout", "-b", $upstreamBranch, "$remote/$upstreamBranch"
    ) -Description "Create pristine upstream mirror")
    if ([int]$lock.patchCount -gt 0) {
        $bundle = Join-Path $stack ([string]$lock.historyBundle.file)
        [void](Invoke-GitChecked -Repository $staging -Arguments @(
            "bundle", "verify", $bundle
        ) -Description "Verify personal history prerequisites")
        $ref = "refs/heads/$personalBranch"
        $heads = (Invoke-GitChecked -Repository $staging -Arguments @(
            "bundle", "list-heads", $bundle, $ref
        ) -Description "Inspect exact personal history ref").Text
        if ($heads -ne "$($lock.integration.commit) $ref") {
            Throw-PatchStackError -Message "Bundle integration ref differs from the lock file."
        }
        [void](Invoke-GitChecked -Repository $staging -Arguments @(
            "fetch", "--no-tags", $bundle, "${ref}:$ref"
        ) -Description "Restore exact personal commit stack")
        [void](Invoke-GitChecked -Repository $staging -Arguments @(
            "checkout", $personalBranch
        ) -Description "Select customized engine branch")
    }
    else {
        [void](Invoke-GitChecked -Repository $staging -Arguments @(
            "checkout", "-b", $personalBranch
        ) -Description "Create empty personal branch")
    }
    Assert-InitializedEngine -Repository $staging -Catalog $stack -Config $config -Lock $lock
    if (Test-Path -LiteralPath $engine) {
        Throw-PatchStackError -Message "Engine destination appeared during initialization; nothing was overwritten." -ExitCode 3
    }
    # Unlike Move-Item, this refuses an existing destination instead of nesting.
    [System.IO.Directory]::Move($staging, $engine)
    $staging = $null
    Write-Output "Independent engine initialized: $engine"
    Write-Output "Branch: $personalBranch ($($lock.integration.commit))"
    exit 0
}
catch {
    [Console]::Error.WriteLine("Engine initialization failed: $($_.Exception.Message)")
    if ($null -ne $staging -and (Test-Path -LiteralPath $staging)) {
        [Console]::Error.WriteLine("Preserved incomplete initialization for inspection: $staging")
    }
    exit (Get-PatchStackExitCode -ErrorRecord $_ -Default 2)
}
