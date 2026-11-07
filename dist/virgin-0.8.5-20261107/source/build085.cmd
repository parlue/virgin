@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%~dp0"
"C:\Program Files (x86)\CMake\bin\cmake.exe" -S . -B build/085-win -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\CMake\bin\cmake.exe" --build build/085-win --parallel 3
set "build_result=%errorlevel%"
popd
exit /b %build_result%
