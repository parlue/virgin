<#
.SYNOPSIS
  Cross compile Virgin for Android with the installed NDK.

.DESCRIPTION
  Uses the NDK CMake toolchain directly (no Android Studio project needed) and
  produces a standalone, stripped binary that DroidFish / Chess for Android can
  import. The binary is fully static (no shared library dependencies).

.EXAMPLE
  .\build_android.ps1                        # arm64-v8a, PIE (default)
  .\build_android.ps1 -Abi armeabi-v7a
  .\build_android.ps1 -Static                # fully static, no dependencies
  .\build_android.ps1 -Abi all -Dist         # all ABIs, copied into dist\ for the app
  .\build_android.ps1 -Abi all -Clean

.NOTES
  Default link mode is position independent (PIE) with only the Android system
  libraries libc/libm/libdl required - that is the most compatible form for
  Android apps. -Static produces a completely self contained binary (ET_EXEC).

  -Dist copies the finished binaries into dist\ with asset friendly names
  (virgin-arm64-v8a, virgin-armeabi-v7a, virgin-x86_64) plus data\bots.json for
  the app's bot list. The engine itself needs no data files at all.
#>
param(
    [ValidateSet('arm64-v8a', 'armeabi-v7a', 'x86_64', 'all')] [string]$Abi = 'arm64-v8a',
    [switch]$Static,
    [switch]$Dist,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
if (-not $root) { $root = Split-Path -Parent $MyInvocation.MyCommand.Path }

$cmake = 'C:\Program Files (x86)\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake).Source }

$ninja = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (-not (Test-Path $ninja)) { $ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source }
if (-not $ninja) { throw 'ninja not found (needed by the NDK toolchain)' }

$sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
$ndkRoot = Join-Path $sdk 'ndk'
if (-not (Test-Path $ndkRoot)) { throw "Android NDK not found under $ndkRoot" }
$ndk = (Get-ChildItem $ndkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$toolchain = Join-Path $ndk 'build\cmake\android.toolchain.cmake'
$binDir = Join-Path $ndk 'toolchains\llvm\prebuilt\windows-x86_64\bin'
$strip = Join-Path $binDir 'llvm-strip.exe'
$readelf = Join-Path $binDir 'llvm-readelf.exe'

$mode = if ($Static) { 'static' } else { 'pie' }
Write-Host "NDK       : $ndk"
Write-Host "toolchain : $toolchain"
Write-Host "link mode : $mode"

$abis = if ($Abi -eq 'all') { @('arm64-v8a', 'armeabi-v7a', 'x86_64') } else { @($Abi) }

foreach ($one in $abis) {
    $suffix = if ($Static) { '-static' } else { '' }
    $build = Join-Path $root "build\android-$one$suffix"
    if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

    Write-Host ''
    Write-Host "=== building $one ==="

    $flags = @(
        '-S', $root,
        '-B', $build,
        '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$ninja",
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        "-DANDROID_ABI=$one",
        '-DANDROID_PLATFORM=android-21',
        '-DANDROID_STL=c++_static',
        '-DCMAKE_BUILD_TYPE=Release'
    )
    if ($Static) { $flags += '-DVIRGIN_STATIC=ON' }
    if ($one -eq 'armeabi-v7a') { $flags += '-DANDROID_ARM_NEON=ON' }

    & $cmake @flags
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed for $one" }

    & $cmake --build $build
    if ($LASTEXITCODE -ne 0) { throw "build failed for $one" }

    $exe = Join-Path $build 'virgin'
    if (-not (Test-Path $exe)) { throw "expected binary not found: $exe" }

    if (Test-Path $strip) { & $strip --strip-all $exe }

    $size = (Get-Item $exe).Length
    Write-Host ("binary : {0}" -f $exe)
    Write-Host ("size   : {0:N0} bytes ({1:N1} KB)" -f $size, ($size / 1KB))

    if (Test-Path $readelf) {
        $headerText = (& $readelf -h $exe 2>$null | Out-String)
        $typeLine = ($headerText -split "`n" | Where-Object { $_ -match 'Type:' } | Select-Object -First 1)
        if ($typeLine) { Write-Host ("elf    : " + $typeLine.Trim()) }

        $dynText = (& $readelf -d $exe 2>$null | Out-String)
        $neededLines = @($dynText -split "`n" | Where-Object { $_ -match 'NEEDED' })
        if ($neededLines.Count -gt 0) {
            Write-Host 'needs  :'
            foreach ($line in $neededLines) { Write-Host ("  " + $line.Trim()) }
        } else {
            Write-Host 'needs  : nothing (self contained)'
        }
    }

    if ($Dist) {
        $distDir = Join-Path $root 'dist'
        New-Item -ItemType Directory -Force -Path $distDir | Out-Null
        $target = Join-Path $distDir "virgin-$one"
        Copy-Item $exe $target -Force
        Write-Host ("dist   : {0}" -f $target)

        # The app reads the bot catalog for its list. It is not part of the
        # binary, so it has to travel with the engine (see dist\README.txt).
        $catalog = Join-Path $root 'data\bots.json'
        if (Test-Path $catalog) {
            Copy-Item $catalog (Join-Path $distDir 'bots.json') -Force
            Write-Host ("dist   : {0}" -f (Join-Path $distDir 'bots.json'))
        } else {
            Write-Host 'dist   : data\bots.json missing - run "virgin --gen-data data" first'
        }
    }
}
