<#
.SYNOPSIS
  Build Virgin for Windows x64.

.DESCRIPTION
  Uses the installed CMake plus either the Visual Studio 2022 generator
  (default, no environment setup needed) or Ninja with vcvars64.

.EXAMPLE
  .\build_win.ps1
  .\build_win.ps1 -Generator Ninja -Clean
#>
param(
    [ValidateSet('VS', 'Ninja')] [string]$Generator = 'VS',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
if (-not $root) { $root = Split-Path -Parent $MyInvocation.MyCommand.Path }
$build = Join-Path $root 'build\win'

$cmake = 'C:\Program Files (x86)\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake).Source }
if (-not $cmake) { throw 'cmake not found' }

if ($Clean -and (Test-Path $build)) {
    Write-Host "removing $build"
    Remove-Item -Recurse -Force $build
}

if ($Generator -eq 'VS') {
    & $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }
    & $cmake --build $build --config Release
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
} else {
    $ninja = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    $vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $ninja)) { $ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source }
    if (-not $ninja) { throw 'ninja not found' }
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

    $cmd = '"{0}" && "{1}" -S "{2}" -B "{3}" -G Ninja -DCMAKE_MAKE_PROGRAM="{4}" -DCMAKE_BUILD_TYPE=Release && "{1}" --build "{3}"' -f `
        $vcvars, $cmake, $root, $build, $ninja
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
}

$exe = Join-Path $build 'virgin.exe'
if (-not (Test-Path $exe)) { throw "expected binary not found: $exe" }

$size = (Get-Item $exe).Length
Write-Host ''
Write-Host "binary : $exe"
Write-Host ("size   : {0:N0} bytes ({1:N1} KB)" -f $size, ($size / 1KB))
Write-Host ''
Write-Host 'smoke test:'
& $exe --perft 4
