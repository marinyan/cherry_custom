$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build'
[void][IO.Directory]::CreateDirectory($build)
[byte[]]$payload = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot '../note-wheel/wheel-hook.bin'))
$hex = ($payload | ForEach-Object { '0x{0:x2}' -f $_ }) -join ','
[IO.File]::WriteAllText((Join-Path $build 'wheel-payload.h'), "/* Generated from note-wheel/wheel-hook.bin. */`nstatic const unsigned char wheelPayload[] = {$hex};`n")
