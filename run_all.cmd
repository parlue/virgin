@echo off
rem Virgin - komplette Pipeline: Build, Daten, Selbsttest.
rem Aufruf: run_all.cmd
setlocal
cd /d "%~dp0"

echo === 1/3  Windows bauen ===
powershell -NoProfile -ExecutionPolicy Bypass -File build_win.ps1 || goto :error

echo.
echo === 2/3  Daten erzeugen (C++, --gen-data) ===
build\win\virgin.exe --gen-data data || goto :error

echo.
echo === 3/3  Selbsttest (C++, --selftest) ===
build\win\virgin.exe --selftest data || goto :error

echo.
echo FERTIG - alles gruen.
exit /b 0

:error
echo.
echo FEHLGESCHLAGEN (exit %errorlevel%)
exit /b %errorlevel%
