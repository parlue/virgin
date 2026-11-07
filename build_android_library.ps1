param([ValidateSet('arm64-v8a','armeabi-v7a','x86_64','x86','all')][string]$Abi='all')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$cmake='C:\Program Files (x86)\CMake\bin\cmake.exe'
if(-not(Test-Path $cmake)) {$cmake=(Get-Command cmake).Source}
$ninja='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if(-not(Test-Path $ninja)) {$ninja=(Get-Command ninja).Source}
$ndkRoot=Join-Path $env:LOCALAPPDATA 'Android\Sdk\ndk'
$ndk=(Get-ChildItem $ndkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
if(-not $ndk) {throw 'Android NDK missing'}
$dest=Join-Path $root 'dist\virgin-0.8.2-20261104\android'
$abis=if($Abi -eq 'all') {@('arm64-v8a','armeabi-v7a','x86_64','x86')} else {@($Abi)}
foreach($one in $abis) {
  $build=Join-Path $root "build\android-library-$one"
  $flags=@('-S',$root,'-B',$build,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_TOOLCHAIN_FILE=$ndk\build\cmake\android.toolchain.cmake","-DANDROID_ABI=$one",'-DANDROID_PLATFORM=android-21','-DANDROID_STL=c++_static','-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON','-DVIRGIN_LIBRARY=ON','-DCMAKE_BUILD_TYPE=Release')
  if($one -eq 'armeabi-v7a') {$flags+='-DANDROID_ARM_NEON=ON'}
  & $cmake @flags
  if($LASTEXITCODE -ne 0) {throw "Configure failed: $one"}
  & $cmake --build $build -j 4
  if($LASTEXITCODE -ne 0) {throw "Build failed: $one"}
  $lib=Join-Path $build 'libvirgin.so'
  $jni=Join-Path $dest "jniLibs\$one"
  New-Item -ItemType Directory -Force -Path $jni | Out-Null
  Copy-Item -LiteralPath $lib -Destination (Join-Path $jni 'libvirgin.so') -Force
  Write-Host "$one : $((Get-Item $lib).Length) bytes"
}
