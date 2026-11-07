@echo off
rem Virgin gegen Stockfish 18 - Elo-Treppe.
rem Vorgabe: 1 Sekunde pro Zug, 4 Partien je Stufe (2x Weiss, 2x Schwarz),
rem Stockfish startet bei Elo 2300 und wird je Stufe um 50 erhoeht.
rem Aufruf: run_match.cmd [startElo] [schritt] [partien] [ms/zug] [maxElo] [bot] [stufen|up] [gegnerMs]
rem Ohne GegnerMs sucht die Engine selbst: erst Elo runter bis 1320, danach die
rem Bedenkzeit des Gegners halbieren (1000 -> 500 -> 250 -> ... -> 30 ms), bis
rem das Ergebnis bei 50 % liegt. Stufen, die nicht mehr zu gewinnen sind,
rem werden sofort abgebrochen.
setlocal
cd /d "%~dp0"

set "SF=%USERPROFILE%\Documents\engines\stockfish18.exe"
if not exist "%SF%" set "SF=%USERPROFILE%\Documents\engines\stockfish\stockfish18.exe"
if not exist "%SF%" (
  echo Stockfish 18 nicht gefunden. Bitte Pfad in run_match.cmd anpassen.
  exit /b 1
)

set "START=%~1"
if "%START%"=="" set "START=2300"
set "STEP=%~2"
if "%STEP%"=="" set "STEP=50"
set "GAMES=%~3"
if "%GAMES%"=="" set "GAMES=4"
set "MS=%~4"
if "%MS%"=="" set "MS=1000"
set "MAXELO=%~5"
if "%MAXELO%"=="" set "MAXELO=2800"
set "BOT=%~6"
if "%BOT%"=="" set "BOT=v2800-ausgeglichen"
set "LEVELS=%~7"
if "%LEVELS%"=="" set "LEVELS=10"
rem Bedenkzeit des Gegners in ms. Leer lassen = Gegner bekommt dieselbe Zeit
rem wie wir und die Zeitachse wird von der Suche selbst abgetastet.
set "OPPMS=%~8"
set "OPPARG="
if not "%OPPMS%"=="" set "OPPARG=%OPPMS%"

if not exist build\win\virgin.exe (
  echo build\win\virgin.exe fehlt - bitte zuerst run_all.cmd ausfuehren.
  exit /b 1
)
if not exist reports mkdir reports

echo Stockfish : %SF%
echo Stufen    : Start %START%, Schritt %STEP%, %GAMES% Partien je Stufe, %MS% ms/Zug, max %LEVELS% Stufen
if not "%OPPARG%"=="" echo Gegnerzeit: %OPPARG% ms/Zug
echo Bot       : %BOT%
echo.

build\win\virgin.exe --match "%SF%" %START% %STEP% %GAMES% %MS% %MAXELO% %BOT% reports %LEVELS% %OPPARG%
exit /b %errorlevel%
