# Virgin - UCI chess engine
# Measures how fast a running match actually advances: plies per second and the
# CPU time of the participating engine processes. Use it to check whether a
# measurement run is engine bound or waiting somewhere.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\match_rate.ps1 -Log reports\ab.log
param(
    [string]$Log = "$PSScriptRoot\..\reports\ab-material.log",
    [int]$Seconds = 30
)

$log = $Log
if (-not (Test-Path $log)) { throw "log not found: $log" }

$size1 = (Get-Item $log).Length
$ply1  = (Select-String -Path $log -Pattern 'ply ' -ErrorAction SilentlyContinue | Measure-Object).Count
$cpu1  = @{}
Get-Process -Name 'virgin*' -ErrorAction SilentlyContinue | ForEach-Object { $cpu1[$_.ProcessName + '#' + $_.Id] = $_.CPU }

Start-Sleep -Seconds $Seconds

$size2 = (Get-Item $log).Length
$ply2  = (Select-String -Path $log -Pattern 'ply ' -ErrorAction SilentlyContinue | Measure-Object).Count
$out = @()
$out += "window            : $Seconds s"
$out += "log bytes         : $size1 -> $size2  (+$($size2 - $size1))"
$out += "plies in log      : $ply1 -> $ply2  (+$($ply2 - $ply1), that is $([math]::Round(($ply2 - $ply1) / [double]$Seconds, 2)) ply/s)"
if (($ply2 - $ply1) -gt 0) {
    $out += "wall clock per ply: $([math]::Round($Seconds / [double]($ply2 - $ply1), 2)) s"
}
Get-Process -Name 'virgin*' -ErrorAction SilentlyContinue | ForEach-Object {
    $k = $_.ProcessName + '#' + $_.Id
    $d = if ($cpu1.ContainsKey($k)) { [math]::Round($_.CPU - $cpu1[$k], 1) } else { 'n/a' }
    $out += "process $($_.ProcessName) (pid $($_.Id)): cpu +$d s in $Seconds s"
}
$out += "(the log is block buffered, so the plies shown lag behind reality)"
$out | Set-Content -Encoding utf8 (Join-Path $PSScriptRoot '..\reports\rate.txt')
