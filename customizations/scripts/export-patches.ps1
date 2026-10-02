[CmdletBinding()]
param(
    [string]$EnginePath,
    [string]$StackPath,
    [switch]$Replace
)

if ([string]::IsNullOrWhiteSpace($EnginePath)) {
    $EnginePath = Join-Path $PSScriptRoot "..\..\engine"
}
if ([string]::IsNullOrWhiteSpace($StackPath)) {
    $StackPath = Join-Path $PSScriptRoot ".."
}

. (Join-Path $PSScriptRoot "lib\PatchStack.Common.ps1")

$staging = $null
$backup = $null

try {
    Assert-GitAvailable
    $engine = Resolve-PatchStackPath -Path $EnginePath -MustExist
    $stack = Resolve-PatchStackPath -Path $StackPath -MustExist
    Assert-EngineGitRepository -Repository $engine
    Assert-GitRepository -Repository $stack
    Assert-NoGitOperation -Repository $engine
    Assert-CleanGitWorktree -Repository $engine

    $config = Get-PatchStackConfig -StackPath $stack
    [void](Assert-PatchStackRemote -EnginePath $engine -Config $config)
    $branch = Get-GitBranch -Repository $engine
    if ($branch -ne [string]$config.integrationBranch) {
        Throw-PatchStackError -Message (
            "Export requires integration branch '$($config.integrationBranch)', current branch is '$branch'."
        ) -ExitCode 3
    }

    $baseRef = Get-PatchStackShortRef -Config $config
    $baseCommit = Resolve-GitCommit -Repository $engine -Reference $baseRef
    $headCommit = Resolve-GitCommit -Repository $engine -Reference "HEAD"
    $ancestor = Invoke-GitCapture -Repository $engine -Arguments @(
        "merge-base", "--is-ancestor", $baseCommit, $headCommit
    )
    if ($ancestor.ExitCode -ne 0) {
        Throw-PatchStackError -Message (
            "Upstream $baseRef is not an ancestor of $branch. " +
            "Rebase the personal branch before exporting; do not export a merged or unrelated history."
        ) -ExitCode 3
    }

    $commitsResult = Invoke-GitChecked -Repository $engine -Arguments @(
        "rev-list", "--reverse", "--topo-order", "$baseCommit..$headCommit"
    ) -Description "List personal commits"
    $commits = @($commitsResult.Lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $topicMap = Get-PatchStackTopicMap -Config $config

    $staging = Join-Path $stack (".patch-stack-staging-" + [guid]::NewGuid().ToString("N"))
    $stagingPatches = Join-Path $staging "patches"
    [void](New-Item -ItemType Directory -Path $stagingPatches -Force)
    Write-Utf8NoBom -Path (Join-Path $stagingPatches ".gitkeep") -Content ""

    $entries = New-Object System.Collections.ArrayList
    $series = New-Object System.Collections.ArrayList
    $seenTopics = @{}
    $position = 0
    $trailerName = [regex]::Escape([string]$config.commitTopicTrailer)
    $trailerPattern = "(?m)^$trailerName`:[ `t]*(?<id>[a-z0-9][a-z0-9.-]*)[ `t]*`r?$"

    foreach ($commit in $commits) {
        $position++
        $message = (Invoke-GitChecked -Repository $engine -Arguments @(
            "show", "-s", "--format=%B", $commit
        ) -Description "Read commit message").Text
        $matches = [regex]::Matches($message, $trailerPattern)
        if ($matches.Count -ne 1) {
            Throw-PatchStackError -Message (
                "Commit $commit must contain exactly one '$($config.commitTopicTrailer): <topic-id>' trailer."
            )
        }
        $topicId = $matches[0].Groups["id"].Value
        if (-not $topicMap.ContainsKey($topicId)) {
            Throw-PatchStackError -Message "Commit $commit references unknown topic '$topicId'."
        }

        $topic = $topicMap[$topicId]
        if (-not $seenTopics.ContainsKey($topicId)) {
            foreach ($dependency in @($topic.dependsOn)) {
                if (-not $seenTopics.ContainsKey([string]$dependency)) {
                    Throw-PatchStackError -Message (
                        "Topic '$topicId' appears before dependency '$dependency' in commit order."
                    )
                }
            }
            $topicReadme = Join-Path (Join-Path $stack "topics") (
                ([string]$topic.path).Replace("/", [System.IO.Path]::DirectorySeparatorChar)
            )
            $topicReadme = Join-Path $topicReadme "README.md"
            if (-not (Test-Path -LiteralPath $topicReadme -PathType Leaf)) {
                Throw-PatchStackError -Message (
                    "Topic documentation is missing for '$topicId': $topicReadme"
                )
            }
            $seenTopics[$topicId] = $true
        }

        $onePatchDirectory = Join-Path $staging ("one-" + $position)
        [void](New-Item -ItemType Directory -Path $onePatchDirectory)
        $formatResult = Invoke-GitChecked -Repository $engine -Arguments @(
            "format-patch",
            "-1",
            "--full-index",
            "--binary",
            "--no-signature",
            "--base=$baseCommit",
            "--output-directory",
            $onePatchDirectory,
            $commit
        ) -Description "Export commit $commit"
        $generated = @(Get-ChildItem -LiteralPath $onePatchDirectory -File -Filter "*.patch")
        if ($generated.Count -ne 1) {
            Throw-PatchStackError -Message (
                "Expected one patch for commit $commit, found $($generated.Count). " +
                $formatResult.Text
            )
        }

        $topicOutput = Join-Path $stagingPatches (
            ([string]$topic.path).Replace("/", [System.IO.Path]::DirectorySeparatorChar)
        )
        [void](New-Item -ItemType Directory -Path $topicOutput -Force)
        $generatedName = $generated[0].Name -replace "^\d{4}-", ""
        $fileName = ("{0:D4}-{1}" -f $position, $generatedName)
        $destination = Join-Path $topicOutput $fileName
        Move-Item -LiteralPath $generated[0].FullName -Destination $destination
        Remove-Item -LiteralPath $onePatchDirectory -Force

        $relativeFile = "patches/" + (ConvertTo-ForwardSlashPath -Path (
            ([string]$topic.path).Trim("/") + "/" + $fileName
        ))
        $subject = (Invoke-GitChecked -Repository $engine -Arguments @(
            "show", "-s", "--format=%s", $commit
        ) -Description "Read commit subject").Text
        $entry = [ordered]@{
            position = $position
            topic = $topicId
            sourceCommit = $commit
            subject = $subject
            file = $relativeFile
            sha256 = Get-FileSha256 -Path $destination
        }
        [void]$entries.Add([pscustomobject]$entry)
        [void]$series.Add($relativeFile)
    }

    $seriesContent = ""
    if ($series.Count -gt 0) {
        $seriesContent = (@($series) -join "`n") + "`n"
    }
    Write-Utf8NoBom -Path (Join-Path $staging "series.txt") -Content $seriesContent

    $shallow = (Invoke-GitChecked -Repository $engine -Arguments @(
        "rev-parse", "--is-shallow-repository"
    ) -Description "Read shallow repository state").Text -eq "true"
    $lock = [ordered]@{
        schemaVersion = 1
        configSha256 = Get-FileSha256 -Path (Join-Path $stack "stack.json")
        upstream = [ordered]@{
            remote = [string]$config.upstream.remote
            branch = [string]$config.upstream.branch
            ref = $baseRef
            commit = $baseCommit
        }
        integration = [ordered]@{
            branch = [string]$config.integrationBranch
            commit = $headCommit
        }
        generatedAtUtc = [DateTime]::UtcNow.ToString("o")
        sourceRepositoryWasShallow = $shallow
        patchCount = $entries.Count
        patches = @($entries)
    }
    if ($commits.Count -gt 0) {
        $bundlePath = Join-Path $staging "personal-history.bundle"
        [void](Invoke-GitChecked -Repository $engine -Arguments @(
            "bundle", "create", $bundlePath, "$baseCommit..refs/heads/$branch"
        ) -Description "Export incremental personal commit history")
        $lock.historyBundle = [ordered]@{
            file = "personal-history.bundle"
            sha256 = Get-FileSha256 -Path $bundlePath
        }
    }
    $lockJson = $lock | ConvertTo-Json -Depth 8
    Write-Utf8NoBom -Path (Join-Path $staging "stack.lock.json") `
        -Content ($lockJson + "`n")

    $outputs = @("patches", "series.txt", "stack.lock.json", "personal-history.bundle")
    $existing = @($outputs | Where-Object { Test-Path -LiteralPath (Join-Path $stack $_) })
    if ($existing.Count -gt 0 -and -not $Replace) {
        Throw-PatchStackError -Message (
            "Generated outputs already exist ($($existing -join ', ')). " +
            "Use -Replace after reviewing the source branch."
        ) -ExitCode 3
    }

    $backedUp = New-Object System.Collections.ArrayList
    $installedOutputs = New-Object System.Collections.ArrayList
    try {
        if ($existing.Count -gt 0) {
            $backup = Join-Path $stack (".patch-stack-backup-" + [guid]::NewGuid().ToString("N"))
            [void](New-Item -ItemType Directory -Path $backup)
            foreach ($name in $existing) {
                Move-Item -LiteralPath (Join-Path $stack $name) -Destination (Join-Path $backup $name)
                [void]$backedUp.Add($name)
            }
        }
        foreach ($name in $outputs) {
            $generatedPath = Join-Path $staging $name
            if (Test-Path -LiteralPath $generatedPath) {
                Move-Item -LiteralPath $generatedPath -Destination (Join-Path $stack $name)
                [void]$installedOutputs.Add($name)
            }
        }
    }
    catch {
        $installError = $_
        $rollbackErrors = New-Object System.Collections.ArrayList
        # Only remove outputs installed by this attempt. Unmoved old outputs
        # must survive a failure half way through the backup phase.
        foreach ($name in $installedOutputs) {
            try {
                $installed = Join-Path $stack $name
                if (Test-Path -LiteralPath $installed) {
                    Remove-Item -LiteralPath $installed -Recurse -Force
                }
            }
            catch { [void]$rollbackErrors.Add($_.Exception.Message) }
        }
        foreach ($name in $backedUp) {
            try {
                $installed = Join-Path $stack $name
                if (Test-Path -LiteralPath $installed) {
                    throw "Refusing to overwrite an occupied rollback destination: $installed"
                }
                Move-Item -LiteralPath (Join-Path $backup $name) -Destination $installed
            }
            catch { [void]$rollbackErrors.Add($_.Exception.Message) }
        }
        if ($rollbackErrors.Count -gt 0) {
            Throw-PatchStackError -Message (
                "Export failed: $($installError.Exception.Message). Rollback needs inspection: " +
                ($rollbackErrors -join "; ") + ". Preserved backup: $backup"
            )
        }
        if ($null -ne $backup) {
            Remove-PatchStackTemporaryDirectory -StackPath $stack -Path $backup
            $backup = $null
        }
        throw $installError
    }
    if ($null -ne $backup -and (Test-Path -LiteralPath $backup)) {
        Remove-PatchStackTemporaryDirectory -StackPath $stack -Path $backup
        $backup = $null
    }
    Remove-PatchStackTemporaryDirectory -StackPath $stack -Path $staging
    $staging = $null

    Write-Output "Exported $($entries.Count) patch(es)."
    Write-Output "Base: $baseRef ($baseCommit)"
    Write-Output "Head: $branch ($headCommit)"
    exit 0
}
catch {
    $exitCode = Get-PatchStackExitCode -ErrorRecord $_ -Default 2
    [Console]::Error.WriteLine("Patch export failed: $($_.Exception.Message)")
    if ($null -ne $staging -and (Test-Path -LiteralPath $staging)) {
        try {
            Remove-PatchStackTemporaryDirectory -StackPath $stack -Path $staging
        }
        catch {
            [Console]::Error.WriteLine("Staging cleanup also failed: $($_.Exception.Message)")
        }
    }
    if ($null -ne $backup -and (Test-Path -LiteralPath $backup)) {
        [Console]::Error.WriteLine("Preserved generated-output backup at: $backup")
    }
    exit $exitCode
}

