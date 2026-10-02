Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$script:PatchStackExitEnvironment = 2
$script:PatchStackExitUnsafeState = 3

function New-PatchStackException {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,

        [Parameter(Mandatory = $true)]
        [int]$ExitCode
    )

    $exception = New-Object System.InvalidOperationException -ArgumentList $Message
    $exception.Data["PatchStackExitCode"] = $ExitCode
    return $exception
}

function Throw-PatchStackError {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,

        [int]$ExitCode = 2
    )

    throw (New-PatchStackException -Message $Message -ExitCode $ExitCode)
}

function Get-PatchStackExitCode {
    param(
        [Parameter(Mandatory = $true)]
        [System.Management.Automation.ErrorRecord]$ErrorRecord,

        [int]$Default = 2
    )

    $exception = $ErrorRecord.Exception
    while ($null -ne $exception) {
        if ($exception.Data.Contains("PatchStackExitCode")) {
            return [int]$exception.Data["PatchStackExitCode"]
        }
        $exception = $exception.InnerException
    }
    return $Default
}

function Resolve-PatchStackPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [string]$BasePath = (Get-Location).Path,

        [switch]$MustExist
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        $fullPath = [System.IO.Path]::GetFullPath($Path)
    }
    else {
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $BasePath $Path))
    }

    if ($MustExist -and -not (Test-Path -LiteralPath $fullPath)) {
        Throw-PatchStackError -Message "Required path does not exist: $fullPath"
    }

    return $fullPath
}

function Test-PatchStackChildPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Parent,

        [Parameter(Mandatory = $true)]
        [string]$Child
    )

    $parentFull = [System.IO.Path]::GetFullPath($Parent).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )
    $childFull = [System.IO.Path]::GetFullPath($Child)
    $prefix = $parentFull + [System.IO.Path]::DirectorySeparatorChar
    return $childFull.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-PatchStackChildPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Parent,

        [Parameter(Mandatory = $true)]
        [string]$Child,

        [string]$Description = "path"
    )

    if (-not (Test-PatchStackChildPath -Parent $Parent -Child $Child)) {
        Throw-PatchStackError -Message "$Description escapes its allowed root: $Child"
    }
}

function Invoke-GitCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $raw = @(& git -C $Repository @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $lines = @($raw | ForEach-Object { $_.ToString() })
    return [pscustomobject]@{
        ExitCode = $exitCode
        Lines = $lines
        Text = ($lines -join "`n").Trim()
    }
}

function Invoke-GitChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [string]$Description = "Git command"
    )

    $result = Invoke-GitCapture -Repository $Repository -Arguments $Arguments
    if ($result.ExitCode -ne 0) {
        $detail = $result.Text
        if ([string]::IsNullOrWhiteSpace($detail)) {
            $detail = "exit code $($result.ExitCode)"
        }
        Throw-PatchStackError -Message "$Description failed: $detail"
    }
    return $result
}

function Assert-GitAvailable {
    if ($null -eq (Get-Command git -ErrorAction SilentlyContinue)) {
        Throw-PatchStackError -Message "git.exe is not available on PATH."
    }
}

function Assert-GitRepository {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository
    )

    if (-not (Test-Path -LiteralPath $Repository -PathType Container)) {
        Throw-PatchStackError -Message "Git repository directory does not exist: $Repository"
    }

    $result = Invoke-GitCapture -Repository $Repository -Arguments @(
        "rev-parse", "--is-inside-work-tree"
    )
    if ($result.ExitCode -ne 0 -or $result.Text -ne "true") {
        Throw-PatchStackError -Message "Not a Git worktree: $Repository"
    }
}

function Assert-EngineGitRepository {
    param([Parameter(Mandatory = $true)][string]$Repository)

    Assert-GitRepository -Repository $Repository
    $top = (Invoke-GitChecked -Repository $Repository -Arguments @(
        "rev-parse", "--show-toplevel"
    ) -Description "Resolve engine repository root").Text
    $expected = [System.IO.Path]::GetFullPath($Repository).TrimEnd("\", "/")
    $actual = [System.IO.Path]::GetFullPath($top).TrimEnd("\", "/")
    if (-not $expected.Equals($actual, [System.StringComparison]::OrdinalIgnoreCase)) {
        Throw-PatchStackError -Message (
            "Engine must be an independent Git checkout rooted at '$expected', " +
            "not a directory inside '$actual'."
        ) -ExitCode 3
    }
}

function Get-GitBranch {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository
    )

    $result = Invoke-GitCapture -Repository $Repository -Arguments @(
        "symbolic-ref", "--quiet", "--short", "HEAD"
    )
    if ($result.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($result.Text)) {
        Throw-PatchStackError -Message "Detached HEAD is not allowed: $Repository" -ExitCode 3
    }
    return $result.Text
}

function Assert-GitBranchNotCheckedOut {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Reference
    )

    $worktrees = Invoke-GitChecked -Repository $Repository -Arguments @(
        "worktree", "list", "--porcelain"
    ) -Description "Check upstream mirror worktree ownership"
    if ($worktrees.Lines -ccontains "branch $Reference") {
        Throw-PatchStackError -Message (
            "Cannot advance '$Reference' while it is checked out in a worktree."
        ) -ExitCode 3
    }
}

function Get-GitDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository
    )

    $result = Invoke-GitChecked -Repository $Repository -Arguments @(
        "rev-parse", "--absolute-git-dir"
    ) -Description "Resolve Git directory"
    return [System.IO.Path]::GetFullPath($result.Text)
}

function Assert-NoGitOperation {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository
    )

    $gitDirectory = Get-GitDirectory -Repository $Repository
    $markers = @(
        "rebase-apply",
        "rebase-merge",
        "MERGE_HEAD",
        "CHERRY_PICK_HEAD",
        "REVERT_HEAD"
    )
    foreach ($marker in $markers) {
        if (Test-Path -LiteralPath (Join-Path $gitDirectory $marker)) {
            Throw-PatchStackError -Message (
                "An unfinished Git operation exists ($marker) in $Repository. " +
                "Continue or abort it before running patch-stack automation."
            ) -ExitCode 3
        }
    }
}

function Assert-CleanGitWorktree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository
    )

    $result = Invoke-GitChecked -Repository $Repository -Arguments @(
        "status", "--porcelain=v1", "--untracked-files=normal"
    ) -Description "Read Git worktree status"
    if (-not [string]::IsNullOrWhiteSpace($result.Text)) {
        Throw-PatchStackError -Message (
            "Git worktree must be clean before this operation: $Repository`n" +
            $result.Text
        ) -ExitCode 3
    }
}

function Resolve-GitCommit {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Repository,

        [Parameter(Mandatory = $true)]
        [string]$Reference
    )

    $result = Invoke-GitCapture -Repository $Repository -Arguments @(
        "rev-parse", "--verify", "$Reference^{commit}"
    )
    if ($result.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($result.Text)) {
        Throw-PatchStackError -Message (
            "Git commit reference is unavailable: $Reference. " +
            "If this is a shallow clone, fetch or deepen the required history first."
        )
    }
    return $result.Text
}

function Get-PatchStackConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StackPath
    )

    $configPath = Join-Path $StackPath "stack.json"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        Throw-PatchStackError -Message "Patch stack configuration is missing: $configPath"
    }

    try {
        $config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        Throw-PatchStackError -Message "Invalid stack.json: $($_.Exception.Message)"
    }

    $required = @(
        "schemaVersion",
        "upstream",
        "integrationBranch",
        "commitTopicTrailer",
        "topics"
    )
    foreach ($name in $required) {
        if ($config.PSObject.Properties.Name -notcontains $name) {
            Throw-PatchStackError -Message "stack.json is missing required property: $name"
        }
    }

    if ([int]$config.schemaVersion -ne 1) {
        Throw-PatchStackError -Message "Unsupported stack.json schemaVersion: $($config.schemaVersion)"
    }
    if ([string]::IsNullOrWhiteSpace([string]$config.upstream.remote) -or
        [string]::IsNullOrWhiteSpace([string]$config.upstream.branch)) {
        Throw-PatchStackError -Message "stack.json upstream.remote and upstream.branch are required."
    }
    if ([string]::IsNullOrWhiteSpace([string]$config.integrationBranch)) {
        Throw-PatchStackError -Message "stack.json integrationBranch is required."
    }
    if ([string]$config.commitTopicTrailer -notmatch "^[A-Za-z][A-Za-z0-9-]*$") {
        Throw-PatchStackError -Message "Invalid commitTopicTrailer: $($config.commitTopicTrailer)"
    }

    $topics = @($config.topics)
    $topicIds = @{}
    foreach ($topic in $topics) {
        foreach ($name in @("id", "path", "description", "dependsOn", "verification")) {
            if ($topic.PSObject.Properties.Name -notcontains $name) {
                Throw-PatchStackError -Message "Topic is missing required property '$name'."
            }
        }

        $topicId = [string]$topic.id
        $topicPath = [string]$topic.path
        if ($topicId -notmatch "^[a-z0-9][a-z0-9.-]*$") {
            Throw-PatchStackError -Message "Invalid topic id: $topicId"
        }
        if ($topicPath -notmatch "^[A-Za-z0-9][A-Za-z0-9._/-]*$" -or
            $topicPath.Contains("..") -or $topicPath.Contains("\")) {
            Throw-PatchStackError -Message "Invalid topic path: $topicPath"
        }
        if ($topicIds.ContainsKey($topicId)) {
            Throw-PatchStackError -Message "Duplicate topic id: $topicId"
        }
        $topicIds[$topicId] = $topic
    }

    foreach ($topic in $topics) {
        foreach ($dependency in @($topic.dependsOn)) {
            $dependencyId = [string]$dependency
            if ($dependencyId -eq [string]$topic.id) {
                Throw-PatchStackError -Message "Topic '$($topic.id)' depends on itself."
            }
            if (-not $topicIds.ContainsKey($dependencyId)) {
                Throw-PatchStackError -Message (
                    "Topic '$($topic.id)' depends on unknown topic '$dependencyId'."
                )
            }
        }
    }

    return $config
}

function Get-PatchStackTopicMap {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Config
    )

    $map = @{}
    foreach ($topic in @($Config.topics)) {
        $map[[string]$topic.id] = $topic
    }
    return $map
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [AllowEmptyString()]
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Get-FileSha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function ConvertTo-ForwardSlashPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return $Path.Replace("\", "/")
}

function Get-PatchStackRef {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Config
    )

    return "refs/remotes/$($Config.upstream.remote)/$($Config.upstream.branch)"
}

function Get-PatchStackShortRef {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Config
    )

    return "$($Config.upstream.remote)/$($Config.upstream.branch)"
}

function Assert-PatchStackRemote {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnginePath,

        [Parameter(Mandatory = $true)]
        [object]$Config
    )

    $remote = [string]$Config.upstream.remote
    $remoteResult = Invoke-GitCapture -Repository $EnginePath -Arguments @(
        "remote", "get-url", $remote
    )
    if ($remoteResult.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($remoteResult.Text)) {
        Throw-PatchStackError -Message "Configured Git remote does not exist: $remote"
    }

    $ref = Get-PatchStackRef -Config $Config
    $refResult = Invoke-GitCapture -Repository $EnginePath -Arguments @(
        "show-ref", "--verify", "--quiet", $ref
    )
    if ($refResult.ExitCode -ne 0) {
        Throw-PatchStackError -Message (
            "Configured upstream ref is unavailable: $ref. Fetch it before continuing."
        )
    }

    return $remoteResult.Text
}

function Remove-PatchStackTemporaryDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StackPath,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    Assert-PatchStackChildPath -Parent $StackPath -Child $Path -Description "Temporary directory"
    $name = [System.IO.Path]::GetFileName($Path.TrimEnd("\", "/"))
    if ($name -notmatch "^(\.tmp|\.patch-stack-staging-|\.patch-stack-backup-)") {
        Throw-PatchStackError -Message "Refusing to remove unexpected directory: $Path"
    }
    Remove-Item -LiteralPath $Path -Recurse -Force
}

function Get-PatchStackLock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StackPath
    )

    $lockPath = Join-Path $StackPath "stack.lock.json"
    if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf)) {
        Throw-PatchStackError -Message (
            "Generated lock file is missing: $lockPath. Run export-patches.ps1 first."
        )
    }

    try {
        $lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 |
            ConvertFrom-Json
    }
    catch {
        Throw-PatchStackError -Message "Invalid stack.lock.json: $($_.Exception.Message)"
    }

    foreach ($name in @(
        "schemaVersion", "configSha256", "upstream", "integration",
        "generatedAtUtc", "patchCount", "patches"
    )) {
        if ($lock.PSObject.Properties.Name -notcontains $name) {
            Throw-PatchStackError -Message "stack.lock.json is missing property: $name"
        }
    }
    if ([int]$lock.schemaVersion -ne 1) {
        Throw-PatchStackError -Message (
            "Unsupported stack.lock.json schemaVersion: $($lock.schemaVersion)"
        )
    }
    if ($lock.PSObject.Properties.Name -contains "historyBundle") {
        $history = $lock.historyBundle
        foreach ($name in @("file", "sha256")) {
            if ($history.PSObject.Properties.Name -notcontains $name) {
                Throw-PatchStackError -Message "historyBundle is missing property: $name"
            }
        }
        if ([string]$history.file -ne "personal-history.bundle") {
            Throw-PatchStackError -Message "Unexpected personal history bundle path."
        }
        $bundlePath = Join-Path $StackPath ([string]$history.file)
        if (-not (Test-Path -LiteralPath $bundlePath -PathType Leaf)) {
            Throw-PatchStackError -Message "Personal history bundle is missing: $bundlePath"
        }
        if ((Get-FileSha256 -Path $bundlePath) -ne [string]$history.sha256) {
            Throw-PatchStackError -Message "Personal history bundle SHA-256 mismatch."
        }
    }

    return $lock
}

function Get-PatchStackSeries {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StackPath
    )

    $seriesPath = Join-Path $StackPath "series.txt"
    if (-not (Test-Path -LiteralPath $seriesPath -PathType Leaf)) {
        Throw-PatchStackError -Message "Generated series file is missing: $seriesPath"
    }

    return @(
        Get-Content -LiteralPath $seriesPath -Encoding UTF8 |
            ForEach-Object { $_.Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and -not $_.StartsWith("#") }
    )
}

function Assert-PatchStackCatalog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StackPath,

        [Parameter(Mandatory = $true)]
        [object]$Config
    )

    $lock = Get-PatchStackLock -StackPath $StackPath
    $series = @(Get-PatchStackSeries -StackPath $StackPath)
    $entries = @($lock.patches)
    if ([int]$lock.patchCount -ne $entries.Count) {
        Throw-PatchStackError -Message (
            "stack.lock.json patchCount is $($lock.patchCount), but contains $($entries.Count) entries."
        )
    }
    if ($series.Count -ne $entries.Count) {
        Throw-PatchStackError -Message (
            "series.txt contains $($series.Count) patches, lock contains $($entries.Count)."
        )
    }

    $configPath = Join-Path $StackPath "stack.json"
    $configHash = Get-FileSha256 -Path $configPath
    if ($configHash -ne [string]$lock.configSha256) {
        Throw-PatchStackError -Message (
            "stack.json differs from stack.lock.json. Export the patch stack again."
        )
    }

    $topicMap = Get-PatchStackTopicMap -Config $Config
    $expectedFiles = @{}
    $seenTopics = @{}
    for ($index = 0; $index -lt $entries.Count; $index++) {
        $entry = $entries[$index]
        $position = $index + 1
        if ([int]$entry.position -ne $position) {
            Throw-PatchStackError -Message "Patch position mismatch at index $position."
        }

        $relative = ConvertTo-ForwardSlashPath -Path ([string]$entry.file)
        if ($series[$index] -ne $relative) {
            Throw-PatchStackError -Message (
                "series.txt order differs from stack.lock.json at position $position."
            )
        }
        if (-not $relative.StartsWith("patches/", [System.StringComparison]::Ordinal)) {
            Throw-PatchStackError -Message "Patch path must be below patches/: $relative"
        }
        if ($relative.Contains("..") -or $relative.Contains("\")) {
            Throw-PatchStackError -Message "Unsafe patch path in lock file: $relative"
        }

        $patchPath = [System.IO.Path]::GetFullPath((Join-Path $StackPath $relative))
        Assert-PatchStackChildPath -Parent $StackPath -Child $patchPath -Description "Patch path"
        if (-not (Test-Path -LiteralPath $patchPath -PathType Leaf)) {
            Throw-PatchStackError -Message "Patch file is missing: $relative"
        }

        $actualHash = Get-FileSha256 -Path $patchPath
        if ($actualHash -ne [string]$entry.sha256) {
            Throw-PatchStackError -Message "Patch SHA-256 mismatch: $relative"
        }
        if ($expectedFiles.ContainsKey($relative)) {
            Throw-PatchStackError -Message "Duplicate patch path in lock file: $relative"
        }
        $expectedFiles[$relative] = $true

        $topicId = [string]$entry.topic
        if (-not $topicMap.ContainsKey($topicId)) {
            Throw-PatchStackError -Message "Patch references unknown topic: $topicId"
        }
        if (-not $seenTopics.ContainsKey($topicId)) {
            foreach ($dependency in @($topicMap[$topicId].dependsOn)) {
                if (-not $seenTopics.ContainsKey([string]$dependency)) {
                    Throw-PatchStackError -Message (
                        "Topic '$topicId' appears before dependency '$dependency'."
                    )
                }
            }
            $seenTopics[$topicId] = $true
        }
    }

    $patchRoot = Join-Path $StackPath "patches"
    if (-not (Test-Path -LiteralPath $patchRoot -PathType Container)) {
        Throw-PatchStackError -Message "Generated patches directory is missing: $patchRoot"
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $patchRoot -Recurse -File -Filter "*.patch")) {
        $relative = "patches/" + (ConvertTo-ForwardSlashPath -Path (
            $file.FullName.Substring($patchRoot.Length).TrimStart("\", "/")
        ))
        if (-not $expectedFiles.ContainsKey($relative)) {
            Throw-PatchStackError -Message "Untracked generated patch file: $relative"
        }
    }

    return $lock
}

function Invoke-PatchStackVerificationCommands {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EnginePath,

        [Parameter(Mandatory = $true)]
        [object]$Config,

        [Parameter(Mandatory = $true)]
        [object]$Lock
    )

    $activeTopics = @{}
    foreach ($entry in @($Lock.patches)) {
        $activeTopics[[string]$entry.topic] = $true
    }

    foreach ($topic in @($Config.topics)) {
        $topicId = [string]$topic.id
        if (-not $activeTopics.ContainsKey($topicId)) {
            continue
        }

        foreach ($verification in @($topic.verification)) {
            $file = [string]$verification.file
            $arguments = @($verification.arguments | ForEach-Object { [string]$_ })
            # The shipped catalog names this script relative to the conventional
            # engine/ layout. Bind that built-in verifier to this script installation
            # and the selected checkout, even when EnginePath is elsewhere. Leave
            # arbitrary user verification commands and their arguments unchanged.
            $executableName = ($file.Replace("\", "/") -split "/")[-1]
            $isPowerShell = @("powershell.exe", "powershell", "pwsh.exe", "pwsh") -icontains $executableName
            for ($argumentIndex = 0; $isPowerShell -and $argumentIndex -lt ($arguments.Count - 1); $argumentIndex++) {
                if ($arguments[$argumentIndex] -ieq "-File" -and
                    $arguments[$argumentIndex + 1].Replace("\", "/") -ieq
                    "../customizations/scripts/verify-topic.ps1") {
                    $arguments[$argumentIndex + 1] = Join-Path $PSScriptRoot "../verify-topic.ps1"
                    $engineArgument = -1
                    for ($index = 0; $index -lt $arguments.Count; $index++) {
                        if ($arguments[$index] -ieq "-EnginePath") { $engineArgument = $index; break }
                    }
                    if ($engineArgument -ge 0) {
                        if ($engineArgument + 1 -ge $arguments.Count) {
                            Throw-PatchStackError -Message "Missing verification EnginePath argument."
                        }
                        $arguments[$engineArgument + 1] = $EnginePath
                    }
                    else {
                        $arguments += @("-EnginePath", $EnginePath)
                    }
                    break
                }
            }
            $workingDirectory = $EnginePath
            if ($verification.PSObject.Properties.Name -contains "workingDirectory" -and
                -not [string]::IsNullOrWhiteSpace([string]$verification.workingDirectory)) {
                $relativeWorkingDirectory = [string]$verification.workingDirectory
                $workingDirectory = [System.IO.Path]::GetFullPath(
                    (Join-Path -Path $EnginePath -ChildPath $relativeWorkingDirectory)
                )
                Assert-PatchStackChildPath -Parent $EnginePath -Child $workingDirectory `
                    -Description "Verification working directory"
            }
            if (-not (Test-Path -LiteralPath $workingDirectory -PathType Container)) {
                Throw-PatchStackError -Message (
                    "Verification working directory does not exist: $workingDirectory"
                )
            }
            if ($null -eq (Get-Command $file -ErrorAction SilentlyContinue)) {
                Throw-PatchStackError -Message "Verification executable is unavailable: $file"
            }

            Write-Output "Running verification for $topicId`: $file $($arguments -join ' ')"
            Push-Location $workingDirectory
            try {
                & $file @arguments
                $exitCode = $LASTEXITCODE
            }
            finally {
                Pop-Location
            }
            if ($exitCode -ne 0) {
                Throw-PatchStackError -Message (
                    "Verification failed for topic '$topicId' with exit code $exitCode."
                ) -ExitCode 1
            }
        }
    }
}

