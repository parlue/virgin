param([Parameter(Mandatory=$true)][string]$Engine)
$ErrorActionPreference = 'Stop'
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $Engine
$start.WorkingDirectory = Split-Path -Parent $Engine
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $start
[void]$process.Start()
function SendLine([string]$line) { $process.StandardInput.WriteLine($line); $process.StandardInput.Flush() }
function ReadUntil([string]$pattern) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $lines = New-Object 'System.Collections.Generic.List[string]'
    while ($timer.ElapsedMilliseconds -lt 15000) {
        $read = $process.StandardOutput.ReadLineAsync()
        if (-not $read.Wait(15000)) { throw 'UCI output timeout' }
        $line = $read.Result
        if ($null -eq $line) { throw 'Engine terminated unexpectedly' }
        $lines.Add($line)
        if ($line -match $pattern) { return $lines.ToArray() }
    }
    throw 'UCI response timeout'
}
try {
    SendLine 'uci'
    $handshake = ReadUntil '^uciok$'
    if (-not ($handshake -match 'id name Virgin 0.6.0')) { throw 'Wrong engine version' }
    Write-Output 'PASS UCI version and handshake'
    SendLine 'setoption name Threads value 1'
    SendLine 'setoption name UCI_LimitStrength value false'
    SendLine 'setoption name Style value Angriffslustig'
    SendLine 'isready'
    $null = ReadUntil '^readyok$'
    SendLine 'position startpos'
    SendLine 'go depth 7'
    $searched = ReadUntil '^bestmove '
    if (-not ($searched -match '^info depth 7 ')) { throw 'Depth 7 not completed' }
    Write-Output ('PASS single thread depth 7: ' + $searched[-1])
    SendLine 'setoption name Threads value 4'
    SendLine 'position fen 4k3/8/8/3q4/4P3/8/8/4K3 w - - 0 1'
    SendLine 'go depth 5'
    $searched = ReadUntil '^bestmove '
    if ($searched[-1] -notmatch '^bestmove e4d5') { throw ('Missed queen: ' + $searched[-1]) }
    Write-Output 'PASS four thread search captures hanging queen'
    SendLine 'position startpos'
    SendLine 'go infinite'
    SendLine 'setoption name Hash value 4'
    SendLine 'isready'
    $null = ReadUntil '^readyok$'
    Write-Output 'PASS safe option change during active search'
    SendLine 'position startpos'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    SendLine 'go movetime 100'
    $searched = ReadUntil '^bestmove '
    if ($timer.ElapsedMilliseconds -gt 3000) { throw 'Movetime exceeded 3 seconds' }
    Write-Output ('PASS timed search: ' + $timer.ElapsedMilliseconds + ' ms, ' + $searched[-1])
    SendLine 'quit'
    if (-not $process.WaitForExit(5000)) { throw 'Engine did not quit' }
    if ($process.ExitCode -ne 0) { throw 'Engine exited with error' }
    Write-Output 'PASS clean shutdown'
} finally {
    if (-not $process.HasExited) { $process.Kill() }
    $process.Dispose()
}
