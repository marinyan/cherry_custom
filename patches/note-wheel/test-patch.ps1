param(
    [Parameter(Mandatory=$true)][string]$OriginalSource,
    [Parameter(Mandatory=$true)][string]$SharpSource
)
$ErrorActionPreference = 'Stop'
$apply = Join-Path $PSScriptRoot 'apply.ps1'
$testDir = Join-Path $PSScriptRoot ('build/patch-test-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($testDir)
function Assert($condition, $message) { if (!$condition) { throw $message } }
function U16($bytes, $offset) { [BitConverter]::ToUInt16($bytes, $offset) }
function U32($bytes, $offset) { [BitConverter]::ToUInt32($bytes, $offset) }
function Hash($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Reject([scriptblock]$action, $messagePattern) {
    $rejected = $false
    try { & $action } catch {
        if ($_.Exception.Message -notlike $messagePattern) { throw }
        $rejected = $true
    }
    Assert $rejected "Expected rejection: $messagePattern"
}
foreach ($case in @(
    @{ Source=$OriginalSource; Variant='Original'; Name='cherry-wheel.exe' },
    @{ Source=$SharpSource; Variant='ChySharp'; Name='cherry-sharp-wheel.exe' }
)) {
    $source = (Resolve-Path -LiteralPath $case.Source).Path
    $sourceHash = Hash $source
    $output = Join-Path $testDir $case.Name
    & $apply -Source $source -Output $output -Variant $case.Variant
    Assert ((Hash $source) -eq $sourceHash) 'Source was modified.'
    [byte[]]$before = [IO.File]::ReadAllBytes($source)
    [byte[]]$after = [IO.File]::ReadAllBytes($output)
    $pe = U32 $before 60
    $opt = $pe + 24
    $table = $opt + (U16 $before ($pe+20))
    $oldCount = U16 $before ($pe+6)
    $newCount = U16 $after ($pe+6)
    Assert ($newCount -eq $oldCount+4) 'Unexpected added sections.'
    # Every existing code/data/resource byte must survive, except the WinMain
    # pointer. Only PE metadata and previously unused section headers may change.
    [byte[]]$allowed = New-Object byte[] $before.Length
    foreach ($range in @(
        @(($pe+6), 2), @(($opt+56), 4), @(($opt+60), 4), @(($opt+64), 4),
        @(($opt+136), 8), @(0x9db94, 4), @(($table+40*$oldCount), (40*($newCount-$oldCount)))
    )) {
        for ($i=$range[0]; $i -lt $range[0]+$range[1]; $i++) { $allowed[$i]=1 }
    }
    for ($i=0; $i -lt $before.Length; $i++) {
        if (!$allowed[$i] -and $before[$i] -ne $after[$i]) {
            throw ('Existing binary byte changed at 0x{0:X}' -f $i)
        }
    }
    $newCode = $table+40*$oldCount
    $entryRVA = (U32 $after 0x9db94) - (U32 $after ($opt+28))
    Assert ($entryRVA -ge (U32 $after ($newCode+12)) -and
        $entryRVA -lt (U32 $after ($newCode+12))+(U32 $after ($newCode+8))) 'Entry does not target the new wheel code.'
    $outputHash = Hash $output
    & $apply -Source $source -Output $output
    Assert ((Hash $output) -eq $outputHash) 'Reapplication changed output.'
    Reject { & $apply -Source $source -Output $source } 'Source and output must be different*'
    $wrongVariant = if ($case.Variant -eq 'Original') { 'ChySharp' } else { 'Original' }
    Reject { & $apply -Source $source -Output $output -Variant $wrongVariant } 'Variant mismatch*'
    Reject { & $apply -Source $output -Output (Join-Path $testDir 'double.exe') } 'Unsupported executable*'
    $defaultDir = Join-Path $testDir $case.Variant
    [void][IO.Directory]::CreateDirectory($defaultDir)
    $defaultSource = Join-Path $defaultDir 'cherry.exe'
    [IO.File]::WriteAllBytes($defaultSource, $before)
    & $apply -Source $defaultSource
    Assert ((Hash (Join-Path $defaultDir $case.Name)) -eq $outputHash) 'Default output differs.'
    Write-Host "PASS: $($case.Variant) preservation, entry, automatic naming, reapplication and variant checks"
}
$conflict = Join-Path $testDir 'keep.exe'
[IO.File]::WriteAllText($conflict, 'must not overwrite')
$conflictHash = Hash $conflict
Reject { & $apply -Source $SharpSource -Output $conflict } 'A different output already exists*'
Assert ((Hash $conflict) -eq $conflictHash) 'Existing output was overwritten.'
$changed = Join-Path $testDir 'unknown-sharp.exe'
[byte[]]$unknown = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $SharpSource).Path)
$unknown[0xe2c] = $unknown[0xe2c] -bxor 1
[IO.File]::WriteAllBytes($changed, $unknown)
$unknownOutput = Join-Path $testDir 'unsupported.exe'
Reject { & $apply -Source $changed -Output $unknownOutput } 'Unsupported executable*'
Assert (!(Test-Path -LiteralPath $unknownOutput)) 'Unsupported binary produced output.'
Write-Host 'PASS: unsupported modifications and conflicting outputs rejected without writes'
Write-Host "Test artifacts (gitignored): $testDir"
