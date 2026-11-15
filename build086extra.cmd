@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%~dp0"
"C:\Program Files (x86)\CMake\bin\cmake.exe" -S . -B build/086-library -G Ninja -DCMAKE_BUILD_TYPE=Release -DVIRGIN_LIBRARY=ON "-DCMAKE_MAKE_PROGRAM=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\CMake\bin\cmake.exe" --build build/086-library --parallel 3
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\CMake\bin\cmake.exe" -S . -B build/086-compact -G Ninja -DCMAKE_BUILD_TYPE=Release -DVIRGIN_EMBEDDED=ON "-DCMAKE_MAKE_PROGRAM=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if errorlevel 1 exit /b 1
"C:\Program Files (x86)\CMake\bin\cmake.exe" --build build/086-compact --parallel 3
set "build_result=%errorlevel%"
popd
exit /b %build_result%
