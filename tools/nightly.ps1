<#
.SYNOPSIS
  Nightly pipeline for Virgin: build both platforms, verify, report.

.DESCRIPTION
  Runs unattended and writes a log plus a short report. Intended to be started
  by the Windows task scheduler (same pattern as the other PowerShell monitors
  in the workspace):

    schtasks /create /tn "Virgin Nightly" /sc daily /st 02:00 ^
      /tr "powershell -NoProfile -ExecutionPolicy Bypass -File <path>\tools\nightly.ps1 -TuneGames 300 -MatchEngine C:\engines\stockfish18.exe"

  Default scope: build, data generation, self test, self play, Android builds.
  With -TuneGames it also tunes the evaluation (self play + train/validation
  split, the result is only written when the validation error improves) and
  with -MatchEngine it measures the engine against that engine before and after
  the tuning (Spot-Suche, PGN und Log nach reports\).

.EXAMPLE
  .\tools\nightly.ps1
  .\tools\nightly.ps1 -SkipAndroid
  .\tools\nightly.ps1 -TuneGames 300 -MatchEngine C:\Users\me\Documents\engines\stockfish18.exe
#>
param(
    [switch]$SkipAndroid,
    [int]$TuneGames = 0,
    [string]$MatchEngine = '',
    [int]$MatchLevels = 6,
    [int]$MatchMovetime = 1000
)

$ErrorActionPreference = 'Continue'

$root = Split-Path -Parent $PSScriptRoot
$reports = Join-Path $root 'reports'
New-Item -ItemType Directory -Force -Path $reports | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $reports "nightly-$stamp.log"

function Section($text) {
    Write-Host ''
    Write-Host "=== $text ==="
    "=== $text ===" | Out-File -Append -Encoding utf8 $log
}

function Run($label, $block) {
    Section $label
    $output = & $block 2>&1
    $code = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_; $_ | Out-File -Append -Encoding utf8 $log }
    Write-Host "exit=$code"
    "exit=$code" | Out-File -Append -Encoding utf8 $log
    return $code
}

$failures = 0

$exe = Join-Path $root 'build\win\virgin.exe'
$dataDir = Join-Path $root 'data'

if ((Run 'build_win.ps1' { powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'build_win.ps1') }) -ne 0) { $failures++ }

if ((Run 'gen-data' { & $exe '--gen-data' $dataDir }) -ne 0) { $failures++ }

if ((Run 'selftest' { & $exe '--selftest' $dataDir }) -ne 0) { $failures++ }

if ((Run 'selfplay' { & $exe '--selfplay' 'v1500-furchtsam' 'v1500-stier' '60' '5' (Join-Path $reports "selfplay-$stamp.txt") }) -ne 0) { $failures++ }

$tunedFile = Join-Path $dataDir 'weights_tuned.txt'
if ($TuneGames -gt 0) {
    # Exit code 4 means "rejected": the tuning only helped the training set.
    # That is a valid result, not a failure.
    $tuneCode = Run 'tune (texel)' { & $exe '--tune' "$TuneGames" '6' '5' $tunedFile '20' }
    if ($tuneCode -ne 0 -and $tuneCode -ne 4) { $failures++ }
}

if ($MatchEngine -ne '') {
    if ((Run 'match (compiled defaults)' { & $exe '--match' $MatchEngine '1320' '50' '6' "$MatchMovetime" '2800' 'v2800-ausgeglichen' $reports "$MatchLevels" '0' }) -ne 0) { $failures++ }
    if ((Test-Path $tunedFile) -and (Run 'match (tuned weights)' { & $exe '--match' $MatchEngine '1320' '50' '6' "$MatchMovetime" '2800' 'v2800-ausgeglichen' $reports "$MatchLevels" '0' $tunedFile }) -ne 0) { $failures++ }
}

if (-not $SkipAndroid) {
    if ((Run 'build_android.ps1 arm64-v8a' { powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'build_android.ps1') '-Abi' 'arm64-v8a' }) -ne 0) { $failures++ }
    if ((Run 'build_android.ps1 armeabi-v7a' { powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'build_android.ps1') '-Abi' 'armeabi-v7a' }) -ne 0) { $failures++ }
}

Section 'report'
$summary = @(
    "date       : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "failures   : $failures",
    "log        : $log",
    "",
    "# next steps (see docs/ROADMAP.md):",
    "#   M5  tuning: -TuneGames 300 (self play, train/validation split)",
    "#   M5  measurement: -MatchEngine <stockfish18.exe> (spot search, 1 s/move)",
    "#   M6  style report and folding the tuned baseline into the bot styles"
)
$summary | ForEach-Object { Write-Host $_; $_ | Out-File -Append -Encoding utf8 $log }

if ($failures -eq 0) {
    Write-Host ''
    Write-Host 'NIGHTLY: PASSED'
} else {
    Write-Host ''
    Write-Host "NIGHTLY: FAILED ($failures step(s))"
}
exit $failures
