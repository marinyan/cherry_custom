param(
    [string]$Source = (Join-Path $PSScriptRoot '../../cherry_1/cherry.exe'),
    [string]$Output,
    [ValidateSet('Auto', 'Original', 'ChySharp')]
    [string]$Variant = 'Auto'
)
$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
[byte[]]$original = [IO.File]::ReadAllBytes($sourcePath)
# Hash the bytes that will actually be patched, rather than reopening the path.
$hasher = [Security.Cryptography.SHA256]::Create()
try { $sourceHash = [BitConverter]::ToString($hasher.ComputeHash($original)).Replace('-', '') }
finally { $hasher.Dispose() }
$profiles = @{
    'B562CB56BAFF6651DFCA0A14A79B19E261E4282319A5911D3238E56186B1CD54' = @{
        Variant = 'Original'; Main = 0x48bd5c; OutputName = 'cherry-wheel.exe'
    }
    '659FA2948FC22689E2D87C0F36E33A504E9D94B24693D4E3315F5904E66C9FB3' = @{
        Variant = 'ChySharp'; Main = 0x4e8200; OutputName = 'cherry-sharp-wheel.exe'
    }
}
$profile = $profiles[$sourceHash]
if (!$profile) {
    throw 'Unsupported executable. Use original Cherry 1.4.3 or the verified chysharp all-options-enabled build. See README for SHA-256 hashes.'
}
if ($Variant -ne 'Auto' -and $Variant -ne $profile.Variant) {
    throw "Variant mismatch: requested $Variant, detected $($profile.Variant). No files changed."
}
if (!$Output) { $Output = Join-Path (Split-Path -Parent $sourcePath) $profile.OutputName }
$outputPath = [IO.Path]::GetFullPath($Output)
if ($sourcePath -eq $outputPath) { throw 'Source and output must be different. The original is never overwritten.' }
Write-Host "Detected: $($profile.Variant)"
[byte[]]$payload = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'wheel-hook.bin'))
function U16($bytes, $offset) { [BitConverter]::ToUInt16($bytes, $offset) }
function U32($bytes, $offset) { [BitConverter]::ToUInt32($bytes, $offset) }
function Put16($bytes, $offset, [uint16]$value) { [BitConverter]::GetBytes($value).CopyTo($bytes, $offset) }
function Put32($bytes, $offset, [uint32]$value) { [BitConverter]::GetBytes($value).CopyTo($bytes, $offset) }
function Align($value, $alignment) { [int]([Math]::Ceiling($value / [double]$alignment) * $alignment) }
function ReadPE($bytes) {
    $pe = U32 $bytes 60
    if ((U32 $bytes $pe) -ne 0x4550 -or (U16 $bytes ($pe + 4)) -ne 0x14c) { throw 'Expected x86 PE.' }
    $opt = $pe + 24
    if ((U16 $bytes $opt) -ne 0x10b) { throw 'Expected PE32.' }
    $table = $opt + (U16 $bytes ($pe + 20))
    $sections = @()
    for ($i = 0; $i -lt (U16 $bytes ($pe + 6)); $i++) {
        $s = $table + 40 * $i
        $sections += [pscustomobject]@{
            Name = [Text.Encoding]::ASCII.GetString($bytes, $s, 8).Trim([char]0)
            VirtualSize = (U32 $bytes ($s + 8)); RVA = (U32 $bytes ($s + 12))
            RawSize = (U32 $bytes ($s + 16)); Raw = (U32 $bytes ($s + 20))
            Flags = (U32 $bytes ($s + 36))
        }
    }
    [pscustomobject]@{ PE=$pe; Opt=$opt; Table=$table; Sections=$sections }
}
function RawOffset($info, $rva) {
    foreach ($s in $info.Sections) {
        if ($rva -ge $s.RVA -and $rva -lt $s.RVA + $s.RawSize) { return [int]($s.Raw + $rva - $s.RVA) }
    }
    throw ('Unmapped RVA: {0:X}' -f $rva)
}
$old = ReadPE $original
$new = ReadPE $payload
$shift = (U32 $original ($old.Opt + 56)) - ($new.Sections | Measure-Object RVA -Minimum).Minimum
if ((U32 $payload ($new.Opt + 28)) -ne (U32 $original ($old.Opt + 28))) { throw 'Image bases differ.' }
if ((U32 $payload ($new.Opt + 104)) -ne 0) { throw 'Payload must not import DLLs.' }
if ((U32 $original 0x9db94) -ne $profile.Main) { throw 'Source WinMain signature mismatch.' }
$fileAlignment = U32 $original ($old.Opt + 36)
$sectionAlignment = U32 $original ($old.Opt + 32)
$oldRelocRVA = U32 $original ($old.Opt + 136)
$oldRelocSize = U32 $original ($old.Opt + 140)
$oldRelocRaw = RawOffset $old $oldRelocRVA
$newRelocRVA = U32 $payload ($new.Opt + 136)
$newRelocSize = U32 $payload ($new.Opt + 140)
$newRelocRaw = RawOffset $new $newRelocRVA
[byte[]]$relocations = New-Object byte[] ($oldRelocSize + $newRelocSize)
[Array]::Copy($original, $oldRelocRaw, $relocations, 0, $oldRelocSize)
[Array]::Copy($payload, $newRelocRaw, $relocations, $oldRelocSize, $newRelocSize)
# Move payload HIGHLOW relocation sites and block RVAs into Cherry's address space.
$position = [int]$oldRelocSize
while ($position -lt $relocations.Length) {
    $page = U32 $relocations $position
    $size = U32 $relocations ($position + 4)
    if ($size -lt 8 -or $position + $size -gt $relocations.Length) { throw 'Invalid relocation block.' }
    for ($j = 8; $j -lt $size; $j += 2) {
        $entry = U16 $relocations ($position + $j)
        $type = $entry -shr 12
        if ($type -eq 0) { continue }
        if ($type -ne 3) { throw 'Unsupported relocation type.' }
        $offset = RawOffset $new ($page + ($entry -band 0xfff))
        Put32 $payload $offset ((U32 $payload $offset) + $shift)
    }
    Put32 $relocations $position ($page + $shift)
    $position += $size
}
$sections = @($new.Sections | Where-Object Name -ne '.reloc')
$headerEnd = $old.Table + 40 * ($old.Sections.Count + $sections.Count + 1)
$firstRaw = ($old.Sections | Measure-Object Raw -Minimum).Minimum
if ($headerEnd -gt $firstRaw) { throw 'Not enough section-header space.' }
$raw = Align $original.Length $fileAlignment
$items = @()
$lastRVA = [int]$shift
$index = 0
foreach ($s in $sections) {
    $items += [pscustomobject]@{ Name=".wh$index"; RVA=($s.RVA+$shift); VirtualSize=$s.VirtualSize; Raw=$raw; RawSize=$s.RawSize; Flags=$s.Flags; Data=$payload; DataOffset=$s.Raw; DataLength=$s.RawSize }
    $raw += $s.RawSize
    $lastRVA = [Math]::Max($lastRVA, (Align ($s.RVA+$shift+[Math]::Max($s.VirtualSize,$s.RawSize)) $sectionAlignment))
    $index++
}
$relocSize = Align $relocations.Length $fileAlignment
$items += [pscustomobject]@{ Name='.whrel'; RVA=$lastRVA; VirtualSize=$relocations.Length; Raw=$raw; RawSize=$relocSize; Flags=0x42000040; Data=$relocations; DataOffset=0; DataLength=$relocations.Length }
[byte[]]$result = New-Object byte[] ($raw + $relocSize)
$original.CopyTo($result, 0)
$header = $old.Table + 40 * $old.Sections.Count
foreach ($item in $items) {
    [Array]::Clear($result, $header, 40)
    [Text.Encoding]::ASCII.GetBytes($item.Name).CopyTo($result, $header)
    Put32 $result ($header+8) $item.VirtualSize
    Put32 $result ($header+12) $item.RVA
    Put32 $result ($header+16) $item.RawSize
    if ($item.RawSize) { Put32 $result ($header+20) $item.Raw }
    Put32 $result ($header+36) $item.Flags
    [Array]::Copy($item.Data, $item.DataOffset, $result, $item.Raw, $item.DataLength)
    $header += 40
}
Put16 $result ($old.PE+6) ($old.Sections.Count+$items.Count)
Put32 $result ($old.Opt+56) (Align ($lastRVA+$relocations.Length) $sectionAlignment)
Put32 $result ($old.Opt+60) (Align $headerEnd $fileAlignment)
Put32 $result ($old.Opt+64) 0
Put32 $result ($old.Opt+136) $lastRVA
Put32 $result ($old.Opt+140) $relocations.Length
# Both variants enter WheelMain, which calls Cherry's original WinMain directly.
# In the ChySharp variant this bypasses ONLY its old wheel-hook initializer;
# all other fixes and the existing .text#/.data#/.rdata# sections are retained.
Put32 $result 0x9db94 ((U32 $original ($old.Opt+28))+$shift+(U32 $payload ($new.Opt+16)))
if (Test-Path -LiteralPath $outputPath) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $resultHash = [BitConverter]::ToString($hasher.ComputeHash($result)).Replace('-', '') }
    finally { $hasher.Dispose() }
    if ((Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash -eq $resultHash) {
        Write-Host "Already applied: $outputPath"
        return
    }
    throw "A different output already exists: $outputPath. Rename it or choose a new -Output path. No files changed."
}
# CreateNew also prevents accidentally replacing a file created during this run.
$stream = [IO.File]::Open($outputPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
try { $stream.Write($result, 0, $result.Length) } finally { $stream.Dispose() }
Write-Host "Created: $outputPath"
Write-Host "Original unchanged: $sourcePath"
