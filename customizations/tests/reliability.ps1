# Sourced by run.ps1 after its isolated upstream/catalog fixtures are ready.
# Never targets a developer's engine or catalog.

# A real rebase conflict must be recoverable without manually moving master.
$beforeConflict = @(Invoke-TestGit -Repository $bootstrapped -Arguments @('rev-parse', 'HEAD'))[0]
Write-TestUtf8NoBom -Path (Join-Path $upstream 'engine.txt') -Content "upstream conflict`n"
[void](Invoke-TestGit -Repository $upstream -Arguments @('add', 'engine.txt'))
[void](Invoke-TestGit -Repository $upstream -Arguments @('commit', '-m', 'Conflicting upstream addition'))
$target = @(Invoke-TestGit -Repository $upstream -Arguments @('rev-parse', 'HEAD'))[0]
$stopped = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack)
Assert-TestEqual 5 $stopped.ExitCode "Expected a real rebase conflict: $($stopped.Text)"
$pendingPath = Join-Path $bootstrapped '.git/patch-stack-update.json'
$pending = Get-Content -LiteralPath $pendingPath -Raw | ConvertFrom-Json
Assert-TestEqual $beforeConflict $pending.originalHead 'Pending state lost the original head.'
$premature = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 3 $premature.ExitCode 'Finalization accepted an unfinished Git operation.'
# Aborting is explicit and must not be mistaken for a completed rebase.
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('rebase', '--abort'))
$abortedFinalize = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 3 $abortedFinalize.ExitCode 'Finalization accepted an aborted rebase.'
Assert-TestEqual $beforeConflict @(Invoke-TestGit -Repository $bootstrapped -Arguments @('rev-parse', 'HEAD'))[0] 'Aborted rebase changed the original branch.'
Assert-TestTrue (Test-Path -LiteralPath $pendingPath) 'Aborted rebase state was silently discarded.'
# The documented explicit cancellation applies only to this disposable fixture.
Remove-Item -LiteralPath $pendingPath
$stoppedAgain = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack)
Assert-TestEqual 5 $stoppedAgain.ExitCode 'Retry after explicit cancellation did not preserve the conflict.'
$pending = Get-Content -LiteralPath $pendingPath -Raw | ConvertFrom-Json
Write-TestUtf8NoBom -Path (Join-Path $bootstrapped 'engine.txt') -Content "resolved personal core`n"
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('add', 'engine.txt'))
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('-c', 'core.editor=true', 'rebase', '--continue'))
$blocked = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack)
Assert-TestEqual 3 $blocked.ExitCode 'A new update ignored the pending update.'
# Never finalize if another actor has replaced the safety reference.
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('update-ref', [string]$pending.backupRef, $target))
$changedSafety = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 3 $changedSafety.ExitCode 'Finalization accepted a changed safety ref.'
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('update-ref', [string]$pending.backupRef, $beforeConflict))
# Simulate the old documented workflow before finalization; it must also recover.
$manualExport = Invoke-TestScript -Name 'export-patches.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Replace')
Assert-TestEqual 0 $manualExport.ExitCode "Manual post-conflict export failed: $($manualExport.Text)"
$finished = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 0 $finished.ExitCode "Finalization failed: $($finished.Text)"
Assert-TestEqual $target @(Invoke-TestGit -Repository $bootstrapped -Arguments @('rev-parse', 'master'))[0] 'Mirror was not finalized.'
Assert-TestTrue (-not (Test-Path -LiteralPath $pendingPath)) 'Successful finalization left pending state.'
Assert-TestEqual $beforeConflict @(Invoke-TestGit -Repository $bootstrapped -Arguments @('rev-parse', [string]$pending.backupRef))[0] 'Safety ref was changed.'
$again = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack)
Assert-TestEqual 0 $again.ExitCode "The next update was blocked: $($again.Text)"
$noPending = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 3 $noPending.ExitCode 'Finalize without pending state was accepted.'
Write-Output 'PASS recovery: conflict, unfinished-operation refusal, manual export, finalize, next update'

# Exercise the shipped verifier via a nonstandard engine and catalog layout.
$fixtureConfig = Get-Content -LiteralPath (Join-Path $stack 'stack.json') -Raw | ConvertFrom-Json
$fixtureConfig.topics[0].verification = @([pscustomobject]@{
    file = $powerShell
    arguments = @('-NoProfile', '-File', '..\customizations\scripts\verify-topic.ps1', '-Topic', 'editor.headless-export-teardown', '-enginepath', 'wrong-checkout')
})
Write-TestUtf8NoBom -Path (Join-Path $stack 'stack.json') -Content (($fixtureConfig | ConvertTo-Json -Depth 8) + "`n")
[void](New-Item -ItemType Directory -Path (Join-Path $bootstrapped 'editor/file_system') -Force)
Write-TestUtf8NoBom -Path (Join-Path $bootstrapped 'editor/editor_node.cpp') -Content "bool EditorNode::is_cmdline_mode()`nreturn singleton == nullptr || singleton->cmdline_mode;`n"
Write-TestUtf8NoBom -Path (Join-Path $bootstrapped 'editor/file_system/editor_file_system.cpp') -Content "EditorNode::is_cmdline_mode()`n"
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('add', 'editor'))
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('commit', '-m', 'Fixture verifier source', '-m', 'Godot-Patch-Topic: core.alpha'))
$verifiedExport = Invoke-TestScript -Name 'export-patches.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Replace')
Assert-TestEqual 0 $verifiedExport.ExitCode "Verifier fixture export failed: $($verifiedExport.Text)"
$selected = Invoke-TestScript -Name 'verify-stack.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-RunTests')
Assert-TestEqual 0 $selected.ExitCode "Selected checkout was not verified: $($selected.Text)"

# Retry both export and verification failure without changing the pending catalog.
$resumeScripts = Join-Path $testRoot 'resume-scripts'
Copy-Item -LiteralPath $scripts -Destination $resumeScripts -Recurse
$failExportMarker = Join-Path $testRoot 'fail-export'
$verificationReady = Join-Path $testRoot 'verification-ready'
$verificationGate = Join-Path $testRoot 'verification-gate.ps1'
Write-TestUtf8NoBom -Path $verificationGate -Content "param([string]`$Ready); if (-not (Test-Path -LiteralPath `$Ready)) { exit 1 }; exit 0"
$fixtureConfig.topics[0].verification += [pscustomobject]@{
    file = $powerShell
    arguments = @('-NoProfile', '-File', $verificationGate, '-Ready', $verificationReady)
}
Write-TestUtf8NoBom -Path (Join-Path $stack 'stack.json') -Content (($fixtureConfig | ConvertTo-Json -Depth 8) + "`n")
$resumeExport = Invoke-TestScript -Name 'export-patches.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Replace')
Assert-TestEqual 0 $resumeExport.ExitCode "Could not prepare resume fixture: $($resumeExport.Text)"
$originalExport = (Join-Path $scripts 'export-patches.ps1').Replace("'", "''")
$escapedMarker = $failExportMarker.Replace("'", "''")
$exportGate = @"
param([string]`$EnginePath, [string]`$StackPath, [switch]`$Replace)
if (Test-Path -LiteralPath '$escapedMarker') { exit 2 }
& '$originalExport' -EnginePath `$EnginePath -StackPath `$StackPath -Replace
exit `$LASTEXITCODE
"@
Write-TestUtf8NoBom -Path (Join-Path $resumeScripts 'export-patches.ps1') -Content $exportGate
Write-TestUtf8NoBom -Path $failExportMarker -Content 'fail'
$exportFailed = Invoke-TestScript -Name 'update-engine.ps1' -ScriptDirectory $resumeScripts -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-RunTests')
Assert-TestEqual 2 $exportFailed.ExitCode 'Update did not report an export failure.'
Assert-TestTrue (Test-Path -LiteralPath $pendingPath) 'Export failure removed pending recovery state.'
Remove-Item -LiteralPath $failExportMarker
$verificationFailed = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 1 $verificationFailed.ExitCode "Expected failed verification: $($verificationFailed.Text)"
Assert-TestTrue (Test-Path -LiteralPath $pendingPath) 'Verification failure removed pending recovery state.'
Write-TestUtf8NoBom -Path $verificationReady -Content 'ready'
$retryFinished = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize', '-RunTests')
Assert-TestEqual 0 $retryFinished.ExitCode "Retry after export and verification failure failed: $($retryFinished.Text)"
Assert-TestTrue (-not (Test-Path -LiteralPath $pendingPath)) 'Verified retry left pending state.'
Write-Output 'PASS recovery retry: failed export and failed verification preserve resumable state'

# A verification request added during finalization must survive later retries too.
Remove-Item -LiteralPath $verificationReady
Write-TestUtf8NoBom -Path $failExportMarker -Content 'fail'
$unverifiedStart = Invoke-TestScript -Name 'update-engine.ps1' -ScriptDirectory $resumeScripts -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack)
Assert-TestEqual 2 $unverifiedStart.ExitCode 'Could not prepare an update without initial RunTests.'
Remove-Item -LiteralPath $failExportMarker
$requestedTests = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize', '-RunTests')
Assert-TestEqual 1 $requestedTests.ExitCode 'Finalization did not run newly requested verification.'
$requiredRetry = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 1 $requiredRetry.ExitCode 'A retry silently dropped verification requested during finalization.'
Assert-TestTrue (Test-Path -LiteralPath $pendingPath) 'Failed required verification removed recovery state.'
Write-TestUtf8NoBom -Path $verificationReady -Content 'ready'
$verifiedRetry = Invoke-TestScript -Name 'update-engine.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Finalize')
Assert-TestEqual 0 $verifiedRetry.ExitCode "Required verification could not finish after repair: $($verifiedRetry.Text)"
Assert-TestTrue (-not (Test-Path -LiteralPath $pendingPath)) 'Successful verified retry left pending state.'
Write-Output 'PASS recovery retry: verification requested during finalization remains required'

Write-TestUtf8NoBom -Path (Join-Path $bootstrapped 'editor/editor_node.cpp') -Content "broken fixture`n"
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('add', 'editor/editor_node.cpp'))
[void](Invoke-TestGit -Repository $bootstrapped -Arguments @('commit', '-m', 'Fixture missing verifier contract', '-m', 'Godot-Patch-Topic: core.alpha'))
$brokenExport = Invoke-TestScript -Name 'export-patches.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-Replace')
Assert-TestEqual 0 $brokenExport.ExitCode "Broken fixture export failed: $($brokenExport.Text)"
$selectedBad = Invoke-TestScript -Name 'verify-stack.ps1' -Arguments @('-EnginePath', $bootstrapped, '-StackPath', $stack, '-RunTests')
Assert-TestEqual 1 $selectedBad.ExitCode 'The verifier did not reject the selected broken checkout.'
Write-Output 'PASS verification: nonstandard EnginePath passes valid source and rejects broken source'

function Get-OutputFingerprint {
    param([string]$Catalog)
    $lines = @()
    foreach ($name in @('patches', 'series.txt', 'stack.lock.json', 'personal-history.bundle')) {
        foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $Catalog $name) -Recurse -File)) {
            $lines += $file.FullName.Substring($Catalog.Length) + ':' + (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    return ($lines | Sort-Object) -join "`n"
}

# Inject failure at every old-output backup and new-output install move.
# The override exists only in an isolated child process, not production code.
$expectedFingerprint = Get-OutputFingerprint $stack
foreach ($failAt in 1..8) {
    $faultStack = Join-Path $testRoot "fault-stack-$failAt"
    Copy-Item -LiteralPath $stack -Destination $faultStack -Recurse
    $wrapper = Join-Path $testRoot "fault-$failAt.ps1"
    $wrapperSource = @'
param([string]$EnginePath, [string]$StackPath, [string]$ExportScript, [int]$FailAt, [switch]$FailRollback)
$script:MoveNumber = 0
function Move-Item {
    param([string]$LiteralPath, [string]$Destination)
    $isBackup = $Destination -match '[\\/]\.patch-stack-backup-[^\\/]+[\\/][^\\/]+$'
    $isInstall = $LiteralPath -match '[\\/]\.patch-stack-staging-[^\\/]+[\\/](patches|series\.txt|stack\.lock\.json|personal-history\.bundle)$'
    if ($isBackup -or $isInstall) {
        $script:MoveNumber++
        if ($script:MoveNumber -eq $FailAt) { throw "Injected transaction move failure $FailAt" }
    }
    if ($FailRollback -and $LiteralPath -match '[\\/]\.patch-stack-backup-[^\\]+[\\/]patches$') { throw "Injected rollback failure" }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
. $ExportScript -EnginePath $EnginePath -StackPath $StackPath -Replace
exit $LASTEXITCODE
'@
    Write-TestUtf8NoBom -Path $wrapper -Content $wrapperSource
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $faultOutput = @(& $powerShell -NoProfile -File $wrapper -EnginePath $bootstrapped -StackPath $faultStack -ExportScript (Join-Path $scripts 'export-patches.ps1') -FailAt $failAt 2>&1)
        $faultExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    Assert-TestEqual 2 $faultExit "Failure injection $failAt did not fail: $faultOutput"
    Assert-TestEqual $expectedFingerprint (Get-OutputFingerprint $faultStack) "Failure $failAt did not restore all original output bytes."
    $valid = Invoke-TestScript -Name 'verify-stack.ps1' -Arguments @('-StackPath', $faultStack, '-SkipBranchComparison')
    Assert-TestEqual 0 $valid.ExitCode "Restored catalog $failAt was invalid: $($valid.Text)"
}
Write-Output 'PASS export rollback: all eight backup/install rename failure boundaries preserve original catalog'

# A rollback failure must leave the original bytes in the reported backup.
$faultStack = Join-Path $testRoot 'fault-rollback'
Copy-Item -LiteralPath $stack -Destination $faultStack -Recurse
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    $faultOutput = @(& $powerShell -NoProfile -File $wrapper -EnginePath $bootstrapped -StackPath $faultStack -ExportScript (Join-Path $scripts 'export-patches.ps1') -FailAt 6 -FailRollback 2>&1)
    $faultExit = $LASTEXITCODE
}
finally { $ErrorActionPreference = $previousPreference }
Assert-TestEqual 2 $faultExit 'Rollback failure was not reported.'
$backups = @(Get-ChildItem -LiteralPath $faultStack -Directory -Force -Filter '.patch-stack-backup-*')
Assert-TestEqual 1 $backups.Count 'Original backup was not retained after rollback failed.'
Assert-TestTrue (Test-Path -LiteralPath (Join-Path $backups[0].FullName 'patches')) 'Original patches were lost after rollback failure.'
Assert-TestTrue (($faultOutput -join "`n") -match 'Preserved backup:') 'Recovery backup path was not reported.'
# Explicit fixture-only restoration proves every original byte remained recoverable.
Move-Item -LiteralPath (Join-Path $backups[0].FullName 'patches') -Destination (Join-Path $faultStack 'patches')
Assert-TestEqual $expectedFingerprint (Get-OutputFingerprint $faultStack) 'Rollback failure lost original bytes.'
Write-Output 'PASS rollback failure: original backup retained, reported, and byte-exactly recoverable'

# A custom command with a coincidentally similar -File argument is not rewritten.
. (Join-Path $scripts 'lib/PatchStack.Common.ps1')
function Test-CustomVerification {
    Assert-TestEqual 4 $args.Count 'Custom command argument count was changed.'
    Assert-TestEqual '-File' $args[0] 'Custom command argument order was changed.'
    Assert-TestEqual '..\customizations\scripts\verify-topic.ps1' $args[1] 'Custom command file argument was changed.'
    Assert-TestEqual '-EnginePath' $args[2] 'Custom command EnginePath option was changed.'
    Assert-TestEqual 'leave-this-alone' $args[3] 'Custom command EnginePath was changed.'
    $global:LASTEXITCODE = 0
}
$customConfig = [pscustomobject]@{ topics = @([pscustomobject]@{
    id = 'custom'
    verification = @([pscustomobject]@{
        file = 'Test-CustomVerification'
        arguments = @('-File', '..\customizations\scripts\verify-topic.ps1', '-EnginePath', 'leave-this-alone')
    })
}) }
$customLock = [pscustomobject]@{ patches = @([pscustomobject]@{ topic = 'custom' }) }
Invoke-PatchStackVerificationCommands -EnginePath $bootstrapped -Config $customConfig -Lock $customLock
Write-Output 'PASS verification: arbitrary custom command arguments remain unchanged'
