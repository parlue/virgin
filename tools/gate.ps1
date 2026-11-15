# Virgin - UCI chess engine
# Runs the full gate (build, --gen-data, --selftest, --bench) and writes a
# readable UTF-8 report. Same steps as run_all.cmd, but the output lands in a
# file instead of the console.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\gate.ps1
param(
    [string]$Report,
    [int]$BenchDepth = 12
)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Report) { $Report = Join-Path $root 'reports\gate.txt' }

$log = New-Object System.Collections.Generic.List[string]
$failed = $false

function Run-Step([string]$title, [scriptblock]$body) {
    $script:log.Add("=== $title ===")
    $out = & $body 2>&1
    $code = $LASTEXITCODE
    foreach ($l in $out) { $script:log.Add([string]$l) }
    $script:log.Add("exit=$code")
    if ($code -ne 0) { $script:failed = $true }
    return $code
}

Run-Step '1/4 build (Windows)' { powershell -NoProfile -ExecutionPolicy Bypass -File build_win.ps1 } | Out-Null
Run-Step '2/4 --gen-data'      { & build\win\virgin.exe --gen-data data } | Out-Null
Run-Step '3/4 --selftest'      { & build\win\virgin.exe --selftest data } | Out-Null
Run-Step '4/4 --bench'         { & build\win\virgin.exe --bench $BenchDepth } | Out-Null

$log.Add('')
$log.Add($(if ($failed) { 'GATE FAILED' } else { 'GATE GREEN' }))
$log | Set-Content -Encoding utf8 -LiteralPath $Report
exit $(if ($failed) { 1 } else { 0 })
