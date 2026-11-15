@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files (x86)\CMake\bin\cmake.exe" -S . -B build/release-0.8.2-library -G Ninja -DCMAKE_BUILD_TYPE=Release -DVIRGIN_LIBRARY=ON -DVIRGIN_LTO=OFF "-DCMAKE_MAKE_PROGRAM=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files (x86)\CMake\bin\cmake.exe" --build build/release-0.8.2-library --parallel 2
exit /b %errorlevel%
