# Virgin - UCI chess engine
# Probes which Arasan tags (github.com/jdart1/arasan-chess, MIT) still carry the
# classical hand written evaluation, and downloads the interesting files into
# ref/arasan-<tag>/ for study. Nothing here is compiled into the engine.
param(
    [string]$Root = "$PSScriptRoot\..",
    [switch]$Download
)

$base = 'https://raw.githubusercontent.com/jdart1/arasan-chess/'
$tags = @('v23.5.0', 'v23.4.0', 'v23.0.0', 'v22.3.0', 'v21.4.0')
$files = @('src/scoring.cpp', 'src/scoring.h', 'src/params.cpp', 'src/search.cpp')

$report = Join-Path $Root 'reports\arasan-tags.txt'
$out = @()
foreach ($t in $tags) {
    $line = $t
    $found = @()
    foreach ($f in $files) {
        try {
            $r = Invoke-WebRequest -UseBasicParsing -Uri ($base + $t + '/' + $f) -ErrorAction Stop
            $line += "  $($f.Split('/')[-1])=$($r.RawContentLength)"
            $found += $f
        } catch {
            $line += "  $($f.Split('/')[-1])=-"
        }
    }
    $out += $line
    if ($Download -and $found.Count -gt 2) {
        $dir = Join-Path $Root "ref\arasan-$t"
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        foreach ($f in $found) {
            $name = $f.Split('/')[-1]
            Invoke-WebRequest -UseBasicParsing -Uri ($base + $t + '/' + $f) -OutFile (Join-Path $dir $name)
        }
        try {
            Invoke-WebRequest -UseBasicParsing -Uri ($base + $t + '/LICENSE') -OutFile (Join-Path $dir 'LICENSE.txt')
        } catch { }
        $out += "    -> downloaded to ref\arasan-$t"
    }
}
$out | Set-Content -Encoding utf8 $report
