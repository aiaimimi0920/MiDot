[CmdletBinding()]
param([string]$TemporaryDirectory = [System.IO.Path]::GetTempPath())

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$scripts = Join-Path $repositoryRoot "scripts"
$powerShell = Join-Path $PSHOME "powershell.exe"
if (-not (Test-Path -LiteralPath $powerShell -PathType Leaf)) {
    $powerShell = (Get-Process -Id $PID).Path
}

function Write-TestUtf8NoBom {
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

function Invoke-TestGit {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$Repository
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        if ([string]::IsNullOrWhiteSpace($Repository)) {
            $raw = @(& git @Arguments 2>&1)
        }
        else {
            $raw = @(& git -C $Repository @Arguments 2>&1)
        }
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        $text = @($raw | ForEach-Object { $_.ToString() }) -join "`n"
        throw "git $($Arguments -join ' ') failed with $exitCode`: $text"
    }
    return @($raw | ForEach-Object { $_.ToString() })
}

function Invoke-TestScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$ScriptDirectory = $scripts
    )

    $scriptPath = Join-Path $ScriptDirectory $Name
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $raw = @(& $powerShell -NoLogo -NoProfile -NonInteractive `
            -ExecutionPolicy Bypass -File $scriptPath @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Text = (@($raw | ForEach-Object { $_.ToString() }) -join "`n")
    }
}

function Assert-TestEqual {
    param(
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if ($Expected -ne $Actual) {
        throw "$Message Expected '$Expected', got '$Actual'."
    }
}

function Assert-TestTrue {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Configure-TestIdentity {
    param([Parameter(Mandatory = $true)][string]$Repository)

    [void](Invoke-TestGit -Repository $Repository -Arguments @(
        "config", "user.name", "Patch Stack Test"
    ))
    [void](Invoke-TestGit -Repository $Repository -Arguments @(
        "config", "user.email", "patch-stack-test@example.invalid"
    ))
}

$temporaryBase = [System.IO.Path]::GetFullPath($TemporaryDirectory).TrimEnd("\")
$testRoot = Join-Path $temporaryBase (
    "godot-patch-stack-tests-" + [guid]::NewGuid().ToString("N")
)
[void](New-Item -ItemType Directory -Path $testRoot)

try {
    $upstream = Join-Path $testRoot "upstream"
    $engine = Join-Path $testRoot "engine"
    $stack = Join-Path $testRoot "stack"
    [void](New-Item -ItemType Directory -Path $upstream)
    [void](New-Item -ItemType Directory -Path $stack)

    [void](Invoke-TestGit -Arguments @("init", "-b", "master", $upstream))
    Configure-TestIdentity -Repository $upstream
    Write-TestUtf8NoBom -Path (Join-Path $upstream "base.txt") -Content "base`n"
    [void](Invoke-TestGit -Repository $upstream -Arguments @("add", "base.txt"))
    [void](Invoke-TestGit -Repository $upstream -Arguments @("commit", "-m", "Add base"))
    $initialBaseLines = @(Invoke-TestGit -Repository $upstream -Arguments @(
        "rev-parse", "HEAD"
    ))
    $initialBase = $initialBaseLines[0].Trim()

    [void](Invoke-TestGit -Arguments @("clone", $upstream, $engine))
    Configure-TestIdentity -Repository $engine
    [void](Invoke-TestGit -Repository $engine -Arguments @(
        "switch", "-c", "personal/main"
    ))

    Write-TestUtf8NoBom -Path (Join-Path $engine "engine.txt") -Content "personal core`n"
    [void](Invoke-TestGit -Repository $engine -Arguments @("add", "engine.txt"))
    $oldAuthorDate = $env:GIT_AUTHOR_DATE
    $oldCommitterDate = $env:GIT_COMMITTER_DATE
    $env:GIT_AUTHOR_DATE = "2020-01-02T03:04:05Z"
    $env:GIT_COMMITTER_DATE = "2020-02-03T04:05:06Z"
    try {
        [void](Invoke-TestGit -Repository $engine -Arguments @(
            "commit", "-m", "Core: Add personal behavior", "-m",
            "Godot-Patch-Topic: core.alpha"
        ))

        Write-TestUtf8NoBom -Path (Join-Path $engine "editor.txt") -Content "personal editor`n"
        [System.IO.File]::WriteAllBytes(
            (Join-Path $engine "payload.bin"),
            [byte[]](0, 1, 2, 10, 13, 128, 255)
        )
        [void](Invoke-TestGit -Repository $engine -Arguments @(
            "add", "editor.txt", "payload.bin"
        ))
        [void](Invoke-TestGit -Repository $engine -Arguments @(
            "commit", "-m", "Editor: Add personal behavior", "-m",
            "Godot-Patch-Topic: editor.beta"
        ))
    }
    finally {
        $env:GIT_AUTHOR_DATE = $oldAuthorDate
        $env:GIT_COMMITTER_DATE = $oldCommitterDate
    }

    [void](Invoke-TestGit -Arguments @("init", "-b", "main", $stack))
    Configure-TestIdentity -Repository $stack
    $config = [ordered]@{
        schemaVersion = 1
        upstream = [ordered]@{ remote = "origin"; branch = "master" }
        integrationBranch = "personal/main"
        commitTopicTrailer = "Godot-Patch-Topic"
        topics = @(
            [ordered]@{
                id = "core.alpha"
                path = "core/alpha"
                description = "Core fixture topic"
                dependsOn = @()
                verification = @()
            },
            [ordered]@{
                id = "editor.beta"
                path = "editor/beta"
                description = "Editor fixture topic"
                dependsOn = @("core.alpha")
                verification = @()
            }
        )
    }
    Write-TestUtf8NoBom -Path (Join-Path $stack "stack.json") `
        -Content (($config | ConvertTo-Json -Depth 8) + "`n")
    foreach ($topicPath in @("core\alpha", "editor\beta")) {
        $directory = Join-Path (Join-Path $stack "topics") $topicPath
        [void](New-Item -ItemType Directory -Path $directory -Force)
        Write-TestUtf8NoBom -Path (Join-Path $directory "README.md") `
            -Content "# Fixture topic`n"
    }

    $export = Invoke-TestScript -Name "export-patches.ps1" -Arguments @(
        "-EnginePath", $engine, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 0 -Actual $export.ExitCode -Message (
        "Initial export failed: $($export.Text)"
    )
    $lock = Get-Content -LiteralPath (Join-Path $stack "stack.lock.json") `
        -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-TestEqual -Expected 2 -Actual ([int]$lock.patchCount) `
        -Message "Exported patch count mismatch."
    Assert-TestEqual -Expected "core.alpha" -Actual ([string]$lock.patches[0].topic) `
        -Message "First topic order mismatch."
    Assert-TestEqual -Expected "editor.beta" -Actual ([string]$lock.patches[1].topic) `
        -Message "Second topic order mismatch."
    Assert-TestTrue -Condition (Test-Path -LiteralPath (
        Join-Path $stack ([string]$lock.historyBundle.file)
    )) -Message "Incremental personal history bundle is missing."
    $catalogOnly = Invoke-TestScript -Name "verify-stack.ps1" -Arguments @(
        "-EnginePath", (Join-Path $testRoot "not-created"), "-StackPath", $stack, "-SkipBranchComparison"
    )
    Assert-TestEqual -Expected 0 -Actual $catalogOnly.ExitCode `
        -Message "Standalone catalog validation unexpectedly required an engine checkout."
    $bundlePath = Join-Path $stack ([string]$lock.historyBundle.file)
    $bundleBytes = [System.IO.File]::ReadAllBytes($bundlePath)
    $damagedBundle = [byte[]]$bundleBytes.Clone()
    $damagedBundle[$damagedBundle.Length - 1] = $damagedBundle[$damagedBundle.Length - 1] -bxor 1
    [System.IO.File]::WriteAllBytes($bundlePath, $damagedBundle)
    $bundleDrift = Invoke-TestScript -Name "verify-stack.ps1" -Arguments @(
        "-StackPath", $stack, "-SkipBranchComparison"
    )
    Assert-TestEqual -Expected 1 -Actual $bundleDrift.ExitCode `
        -Message "Personal history bundle SHA-256 drift was not rejected."
    [System.IO.File]::WriteAllBytes($bundlePath, $bundleBytes)

    $bootstrapped = Join-Path $testRoot "bootstrapped"
    $bootstrap = Invoke-TestScript -Name "initialize-engine.ps1" -Arguments @(
        "-EnginePath", $bootstrapped, "-StackPath", $stack, "-UpstreamUrl", $upstream
    )
    Assert-TestEqual -Expected 0 -Actual $bootstrap.ExitCode -Message (
        "Fresh initialization failed: $($bootstrap.Text)"
    )
    $bootstrapHead = @(Invoke-TestGit -Repository $bootstrapped -Arguments @("rev-parse", "HEAD"))[0]
    Assert-TestEqual -Expected ([string]$lock.integration.commit) -Actual $bootstrapHead `
        -Message "Initialization did not preserve the exact integration commit."
    foreach ($reference in @("master", "origin/master")) {
        $base = @(Invoke-TestGit -Repository $bootstrapped -Arguments @("rev-parse", $reference))[0]
        Assert-TestEqual -Expected $initialBase -Actual $base -Message "Initialized mirror mismatch."
    }
    $bootstrapAgain = Invoke-TestScript -Name "initialize-engine.ps1" -Arguments @(
        "-EnginePath", $bootstrapped, "-StackPath", $stack, "-UpstreamUrl", $upstream
    )
    Assert-TestEqual -Expected 0 -Actual $bootstrapAgain.ExitCode -Message (
        "Repeated initialization failed: $($bootstrapAgain.Text)"
    )
    Write-TestUtf8NoBom -Path (Join-Path $bootstrapped "dirty.txt") -Content "keep me`n"
    $dirtyBootstrap = Invoke-TestScript -Name "initialize-engine.ps1" -Arguments @(
        "-EnginePath", $bootstrapped, "-StackPath", $stack, "-UpstreamUrl", $upstream
    )
    Assert-TestEqual -Expected 3 -Actual $dirtyBootstrap.ExitCode `
        -Message "Initialization did not reject a dirty existing engine."
    Assert-TestTrue -Condition (Test-Path -LiteralPath (Join-Path $bootstrapped "dirty.txt")) `
        -Message "Initialization discarded existing work."
    Remove-Item -LiteralPath (Join-Path $bootstrapped "dirty.txt")

    $flatEngine = Join-Path $stack "flat-engine"
    [void](New-Item -ItemType Directory -Path $flatEngine)
    Write-TestUtf8NoBom -Path (Join-Path $flatEngine "keep.txt") -Content "preserve flat directory`n"
    $flatBootstrap = Invoke-TestScript -Name "initialize-engine.ps1" -Arguments @(
        "-EnginePath", $flatEngine, "-StackPath", $stack, "-UpstreamUrl", $upstream
    )
    Assert-TestEqual -Expected 3 -Actual $flatBootstrap.ExitCode `
        -Message "A flat directory inherited the parent Git repository."
    Assert-TestTrue -Condition (Test-Path -LiteralPath (Join-Path $flatEngine "keep.txt")) `
        -Message "Initialization changed an existing flat directory."

    $verify = Invoke-TestScript -Name "verify-stack.ps1" -Arguments @(
        "-EnginePath", $engine, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 0 -Actual $verify.ExitCode -Message (
        "Initial verification failed: $($verify.Text)"
    )

    $replay = Join-Path $testRoot "replay"
    [void](Invoke-TestGit -Arguments @("clone", $upstream, $replay))
    Configure-TestIdentity -Repository $replay
    $apply = Invoke-TestScript -Name "apply-patches.ps1" -Arguments @(
        "-EnginePath", $replay, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 0 -Actual $apply.ExitCode -Message (
        "Patch application failed: $($apply.Text)"
    )
    Assert-TestEqual -Expected "personal core" -Actual (
        (Get-Content -LiteralPath (Join-Path $replay "engine.txt") -Raw).Trim()
    ) -Message "Text patch content mismatch."
    $expectedBinaryHash = (Get-FileHash -LiteralPath (Join-Path $engine "payload.bin") `
        -Algorithm SHA256).Hash
    $actualBinaryHash = (Get-FileHash -LiteralPath (Join-Path $replay "payload.bin") `
        -Algorithm SHA256).Hash
    Assert-TestEqual -Expected $expectedBinaryHash -Actual $actualBinaryHash `
        -Message "Binary patch content mismatch."
    $replayedTree = @(Invoke-TestGit -Repository $replay -Arguments @("rev-parse", "HEAD^{tree}"))[0]
    $sourceTree = @(Invoke-TestGit -Repository $engine -Arguments @("rev-parse", "HEAD^{tree}"))[0]
    Assert-TestEqual -Expected $sourceTree -Actual $replayedTree `
        -Message "Patch replay did not reconstruct the source tree."

    $dirty = Join-Path $testRoot "dirty"
    [void](Invoke-TestGit -Arguments @("clone", $upstream, $dirty))
    Configure-TestIdentity -Repository $dirty
    Write-TestUtf8NoBom -Path (Join-Path $dirty "dirty.txt") -Content "dirty`n"
    $dirtyApply = Invoke-TestScript -Name "apply-patches.ps1" -Arguments @(
        "-EnginePath", $dirty, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 3 -Actual $dirtyApply.ExitCode -Message (
        "Dirty worktree was not rejected: $($dirtyApply.Text)"
    )

    $conflict = Join-Path $testRoot "conflict"
    [void](Invoke-TestGit -Arguments @("clone", $upstream, $conflict))
    Configure-TestIdentity -Repository $conflict
    Write-TestUtf8NoBom -Path (Join-Path $conflict "engine.txt") -Content "upstream conflict`n"
    [void](Invoke-TestGit -Repository $conflict -Arguments @("add", "engine.txt"))
    [void](Invoke-TestGit -Repository $conflict -Arguments @(
        "commit", "-m", "Create conflicting upstream file"
    ))
    $conflictApply = Invoke-TestScript -Name "apply-patches.ps1" -Arguments @(
        "-EnginePath", $conflict, "-StackPath", $stack, "-AllowBaseMismatch"
    )
    Assert-TestEqual -Expected 4 -Actual $conflictApply.ExitCode -Message (
        "Conflicting patch did not stop with exit 4: $($conflictApply.Text)"
    )
    Assert-TestTrue -Condition (Test-Path -LiteralPath (
        Join-Path $conflict ".git\rebase-apply"
    )) -Message "git am state was not preserved after conflict."
    [void](Invoke-TestGit -Repository $conflict -Arguments @("am", "--abort"))
    $conflictStatus = @(
        Invoke-TestGit -Repository $conflict -Arguments @("status", "--porcelain=v1")
    ) -join "`n"
    Assert-TestEqual -Expected "" -Actual $conflictStatus.Trim() `
        -Message "git am --abort did not restore a clean worktree."

    $firstPatchRelative = [string]$lock.patches[0].file
    $firstPatch = [System.IO.Path]::GetFullPath(
        (Join-Path -Path $stack -ChildPath $firstPatchRelative)
    )
    [System.IO.File]::AppendAllText($firstPatch, "tampered`n")
    $drift = Invoke-TestScript -Name "verify-stack.ps1" -Arguments @(
        "-EnginePath", $engine, "-StackPath", $stack, "-SkipBranchComparison"
    )
    Assert-TestEqual -Expected 1 -Actual $drift.ExitCode -Message (
        "Patch hash drift was not rejected: $($drift.Text)"
    )
    $reexport = Invoke-TestScript -Name "export-patches.ps1" -Arguments @(
        "-EnginePath", $engine, "-StackPath", $stack, "-Replace"
    )
    Assert-TestEqual -Expected 0 -Actual $reexport.ExitCode -Message (
        "Re-export after drift failed: $($reexport.Text)"
    )

    Write-TestUtf8NoBom -Path (Join-Path $upstream "upstream.txt") -Content "new upstream`n"
    [void](Invoke-TestGit -Repository $upstream -Arguments @("add", "upstream.txt"))
    [void](Invoke-TestGit -Repository $upstream -Arguments @(
        "commit", "-m", "Advance upstream"
    ))
    $newUpstreamLines = @(Invoke-TestGit -Repository $upstream -Arguments @(
        "rev-parse", "HEAD"
    ))
    $newUpstream = $newUpstreamLines[0].Trim()
    Assert-TestTrue -Condition ($newUpstream -ne $initialBase) `
        -Message "Fixture upstream did not advance."

    Configure-TestIdentity -Repository $bootstrapped
    $update = Invoke-TestScript -Name "update-engine.ps1" -Arguments @(
        "-EnginePath", $bootstrapped, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 0 -Actual $update.ExitCode -Message (
        "Engine update failed: $($update.Text)"
    )
    $updatedBaseLines = @(Invoke-TestGit -Repository $bootstrapped -Arguments @(
        "rev-parse", "origin/master"
    ))
    $updatedBase = $updatedBaseLines[0].Trim()
    Assert-TestEqual -Expected $newUpstream -Actual $updatedBase `
        -Message "Updated upstream ref mismatch."
    $mirror = @(Invoke-TestGit -Repository $bootstrapped -Arguments @("rev-parse", "master"))[0]
    Assert-TestEqual -Expected $newUpstream -Actual $mirror -Message "Local pristine mirror did not advance."
    $isAncestorRaw = @(& git -C $bootstrapped merge-base --is-ancestor origin/master HEAD 2>&1)
    $isAncestorExit = $LASTEXITCODE
    Assert-TestEqual -Expected 0 -Actual $isAncestorExit `
        -Message "personal/main was not rebased onto origin/master: $($isAncestorRaw -join ' ')"

    $finalVerify = Invoke-TestScript -Name "verify-stack.ps1" -Arguments @(
        "-EnginePath", $bootstrapped, "-StackPath", $stack
    )
    Assert-TestEqual -Expected 0 -Actual $finalVerify.ExitCode -Message (
        "Final verification failed: $($finalVerify.Text)"
    )
    $afterUpdate = Invoke-TestScript -Name "initialize-engine.ps1" -Arguments @(
        "-EnginePath", (Join-Path $testRoot "after-update"),
        "-StackPath", $stack, "-UpstreamUrl", $upstream
    )
    Assert-TestEqual -Expected 0 -Actual $afterUpdate.ExitCode -Message (
        "Initialization from the updated catalog failed: $($afterUpdate.Text)"
    )

    . (Join-Path $PSScriptRoot "reliability.ps1")

    Write-Output "PASS export: ordered text and binary patches"
    Write-Output "PASS initialize: exact commit identity, repeat safety, independent repository boundary"
    Write-Output "PASS apply: exact-base git am --3way reconstruction"
    Write-Output "PASS safety: dirty worktree rejection"
    Write-Output "PASS conflict: preserved git am state and abort recovery"
    Write-Output "PASS integrity: patch and incremental bundle SHA-256 drift detection"
    Write-Output "PASS update: fetch + rebase + re-export + verification"
    Write-Output "All patch-stack integration tests passed."
    exit 0
}
catch {
    [Console]::Error.WriteLine("Patch-stack integration test failed: $($_.Exception.Message)")
    exit 1
}
finally {
    $tempRoot = $temporaryBase
    $resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    $expectedPrefix = Join-Path $tempRoot "godot-patch-stack-tests-"
    if ($resolvedTestRoot.StartsWith(
        $expectedPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -and (Test-Path -LiteralPath $resolvedTestRoot)) {
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

