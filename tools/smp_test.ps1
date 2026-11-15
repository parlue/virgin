# Virgin - UCI chess engine
# Measures Lazy SMP scaling: runs one search per thread count and reports the
# depth, nodes and nodes/s from the last info line. The engine gets its input
# with a delay, otherwise "quit" would stop the search too early.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\smp_test.ps1
param(
    [string]$Engine = "$PSScriptRoot\..\build\win\virgin.exe",
    [int[]]$Threads = @(1, 2, 4, 8),
    [int]$Ms = 3000,
    [string]$Position = "position startpos moves e2e4 e7e5 g1f3 b8c6",
    [string]$Report = "$PSScriptRoot\..\reports\smp-scaling.txt"
)

$lines = @()
foreach ($t in $Threads) {
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo.FileName               = $Engine
    $p.StartInfo.UseShellExecute        = $false
    $p.StartInfo.RedirectStandardInput  = $true
    $p.StartInfo.RedirectStandardOutput = $true
    $p.StartInfo.CreateNoWindow         = $true
    [void]$p.Start()

    $p.StandardInput.WriteLine("uci")
    $p.StandardInput.WriteLine("setoption name Threads value $t")
    $p.StandardInput.WriteLine($Position)
    $p.StandardInput.WriteLine("go movetime $Ms")
    Start-Sleep -Milliseconds ($Ms + 400)
    $p.StandardInput.WriteLine("quit")

    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()

    $info = ($out -split "`n" | Where-Object { $_ -match '^info depth' } | Select-Object -Last 1)
    $best = ($out -split "`n" | Where-Object { $_ -match '^bestmove' } | Select-Object -Last 1)
    $lines += "threads $t : $($info.Trim())"
    $lines += "            $($best.Trim())"
}

$lines | Set-Content -Encoding utf8 $Report
$lines | ForEach-Object { $_ }
