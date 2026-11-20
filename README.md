# Virgin - UCI Schachengine mit Stil-Bots

Entwicklungskandidat: **Virgin 0.8.7 Beta** - [Release-Details](docs/RELEASE_0.8.7.md). Dateien: `dist/virgin-0.8.7-20261120/`.

C++20-UCI-Engine, gebaut für **Bots**: aus Stärkestufe x Spielstil entstehen
fertige Gegner. Alle Bewertungsgewichte sind von außen steuerbar.

* Ziel: 2500-2800 Elo (handgebaute Evaluation, kein NNUE - damit alle Gewichte
  einzeln beeinflussbar bleiben).
* Stilachse "Mut": **Fortress - Stone - Normal - Fire - Thunder**
* Stärke: 700 bis 2800 in 50er-Schritten (oder jede ganze Zahl).
* Katalog: **43 Stufen x 5 Stile = 215 Bots** (`data/bots.json`).
* Windows und Android aus derselben Codebasis, keine externen Bibliotheken,
  Binary ~298 KB (Windows) bzw. ~780 KB (Android, statisch).

## Schnellstart

```powershell
cd C:\Users\DirkSommerfeld\Documents\Projects\virgin

.\run_all.cmd                        # alles auf einmal: Build + Daten + Selbsttest
```

Einzeln – alles über die Engine selbst, kein Hilfsskript nötig:

```powershell
.\build_win.ps1                      # Windows-Binary
.\build_android.ps1                  # Android-Binary (arm64-v8a, PIE)
.\build_android.ps1 -Static          # Android-Binary, voll statisch

.\build\win\virgin.exe --gen-data    # data\ erzeugen (Gewichte, Stile, Elo, Bots)
.\build\win\virgin.exe --selftest    # eingebauter Testlauf
.\build\win\virgin.exe --selfplay v1200-fortress v1200-thunder 60 5
.\run_match.cmd                       # Elo-Treppe gegen Stockfish 18 (1 s/Zug)
.\tools\nightly.ps1                  # Nachtlauf: bauen + prüfen + Report
```

## Eröffnungsbuch und Zugvielfalt

Globales `OwnBook` (Standard **an**) für alle Spielmodi: 90 Varianten bis zehn
Halbzüge, **2.037 Byte Buchdaten**, per `setoption name OwnBook value false`
abschaltbar. Im adaptiven Modus werden sichere gleichwertige Suchzüge innerhalb
von 20 Centipawns zufällig gewählt; erkannte taktische Fortsetzungen haben Vorrang.
Details und Tests: [Eröffnungsbuch](docs/OPENING_BOOK.md).

## UCI-Kommandos

Standard: `uci`, `isready`, `ucinewgame`, `setoption`, `position`, `go`
(`depth`, `nodes`, `movetime`, `wtime/btime/winc/binc/movestogo`, `infinite`,
`ponder`), `stop`, `ponderhit`, `quit`, `debug`.

Zusätzlich: `d` (Brett anzeigen), `eval` (Bewertung mit Aufschlüsselung),
`perft <n>`, `bench <n>`, `compiler`, `sanity` (Konsistenzprüfung),
`weights` (alle Gewichte), `listbots` (Bot-Katalog).

## Testen ohne fremde Software

Alles läuft über die Engine selbst - sie ist ein UCI-Prozess, deine App ist der
Host (siehe `docs/APP_INTEGRATION.md`):

```powershell
.\build\win\virgin.exe --help             # alle Modi
.\build\win\virgin.exe --perft 5          # muss 4865609 liefern
.\build\win\virgin.exe --selftest         # 43 Prüfungen, Exit-Code
.\build\win\virgin.exe --list-bots        # 215 Bots
.\build\win\virgin.exe --gen-data         # data\ neu erzeugen

# Partie zwischen zwei Bot-Profilen, in der Engine selbst
.\build\win\virgin.exe --selfplay v1200-fortress v1200-thunder 60 5
```

`--selfplay` zeigt Zug, Bewertung und Tiefe und schreibt ein Log nach
`reports/`. Stil und Stärke werden dabei wie im UCI-Betrieb angewendet.

## Wettkampf gegen eine fremde Engine (Elo messen / Spot suchen)

`--match` startet eine fremde UCI-Engine als Kindprozess, spielt Partien mit
fester Bedenkzeit und sucht die Stufe, auf der das Ergebnis bei 50 % liegt.
Stockfish 18 kann sich per `UCI_LimitStrength`/`UCI_Elo` begrenzen lassen:

```powershell
# 4 Partien je Stufe (2x Weiss, 2x Schwarz), 1 s/Zug, Start 2300, Schritt 50
.\\run_match.cmd
.\\run_match.cmd 2300 50 4 1000 2800 v2800-normal

# Spot-Suche ab Stockfishs Elo-Untergrenze: erst Elo, dann Gegnerzeit halbieren
.\\build\\win\\virgin.exe --match C:\\Pfad\\stockfish18.exe 2300 50 4 1000 2800 v2800-normal reports 8

# klassische Treppe nur nach oben (statt Spot-Suche)
.\\build\\win\\virgin.exe --match ... reports up
```

Die Engine

* eröffnet jede Partie mit einer von 8 kurzen Eröffnungen (Farbwechsel inklusive),
* prüft jeden Fremdzug gegen die eigene Zugliste (Rochade, En passant, Umwandlung),
* erkennt Matt, Patt, 50-Züge-Regel, dreifache Wiederholung und entscheidet
  nach 110 Zügen per Materialvorteil,
* **bricht eine Stufe sofort ab**, sobald 50 % nicht mehr erreichbar sind
  (zwei Niederlagen statt vier), und hört ganz auf, wenn selbst 30 ms Gegnerzeit
  nicht reichen,
* schreibt jede Partie als PGN nach `reports/match-<bot>-<von>-<bis>-<ms>ms.pgn`,
* meldet am Ende den Spot, z. B.
  `SPOT: 50 % against SF Elo 1320 with 250 .. 500 ms/move (we play 1000 ms)`.

## A/B gegen die eigene Vorgängerversion

Für Bewertungsänderungen ist der Spot-Lauf gegen Stockfish zu grob (wenige
Partien, viel Rauschen). `--match` startet jedes UCI-Programm als Gegner - also
auch die eigene vorherige Version, mit identischer Bedenkzeit:

```powershell
# 1) Baseline sichern (vor dem Umbau!), 2) neu bauen, 3) 100 Partien bei 100 ms
Copy-Item build\win\virgin.exe build\win\virgin_base.exe
.\build_win.ps1
.\build\win\virgin.exe --match build\win\virgin_base.exe 2800 50 100 100 2800 v2800-normal reports 1 100
#                              Gegner                            Stufen  Partien  ms  ...
```

Beide Seiten spielen denselben Code mit derselben Zeit; der Unterschied ist
praktisch nur die Bewertung. Aus dem Gesamtergebnis folgt direkt ein
Elo-Delta (50 % = gleich stark). `Elo 2800` bedeutet hier "kein Handicap".

Der Match schreibt am Ende selbst das Verdikt:

```text
  game 12/100  [03:41]  we play black, opening 6 -> LOSS (188 plies, adjudicated: material down)
  ...
 overall: +28 =5 -25   58 games   score 52.6 %
 verdict: +18 Elo vs 50 % (95 % confidence +/- 52)   in 17.0 min   (3.4 games/min)
```

Zur Einordnung: die 95 %-Schranke ist ungefähr `680 / sqrt(Partien)` Elo
(100 Partien ≈ ±70 Elo), und der Durchsatz liegt bei 100 ms/Zug bei rund
**3,4 Partien pro Minute** (also ~30 Minuten für 100 Partien). Wichtig: während
einer Messung die Maschine nicht anderweitig belasten, sonst konkurrieren die
Engines mit sich selbst.

## Herkunft der Bewertung

Die Bewertung ist handgebaut (kein NNUE), damit alle 91 Gewichte einzeln von
außen steuerbar bleiben. Die **Grundformen der Figurenfeld-Tabellen** kommen
aus den veröffentlichten **PeSTO-Tabellen** (Ronald Friederich, Engine
RofChade/PeSTO; erzeugt von `tools/extract_pesto.ps1` nach `src/pst_pesto.h`).
Die eigenen `Pst.*`-Gewichte liegen als Deltas darüber.

Referenzengines wurden nur auf **Lizenz und Aufbau** geprüft, Quellen liegen
unkompiliert unter `ref/`:

| Engine | Lizenz | Verwendung |
|---|---|---|
| Arasan (Jon Dart) | MIT/Expat | prüfbar, auch Codeübernahme erlaubt |
| PeSTO-Tabellen | veröffentlichte Daten | übernommen, mit Quellenangabe |
| Ethereal, Stockfish | GPL-3.0 | nur Ideen, kein Code |


## Tuning (Texel) und Messung

Die Engine kann ihre eigenen Bewertungsgewichte kalibrieren — ebenfalls als
Engine-Modus, ohne Hilfssprachen:

```powershell
# Selfplay-Daten sammeln, K-Faktor scannen, Gewichte per Koordinatenabstieg fitten
.\build\win\virgin.exe --tune 200 6 5 data\weights_tuned.txt 20
#                 Partien  Tiefe  Passes  Ausgabe            max. Abweichung in %

# Ergebnis messen (Spot-Suche gegen Stockfish, 1 s/Zug, getunte Gewichte laden)
.\build\win\virgin.exe --match C:\Pfad\stockfish18.exe 1320 50 6 1000 2800 v2800-normal reports 8 0 data\weights_tuned.txt

# oder alles unbeaufsichtigt über den Nachtlauf:
.\tools\nightly.ps1 -TuneGames 300 -MatchEngine C:\Pfad\stockfish18.exe
```

Der Tuner teilt die Stellungen deterministisch in **80 % Training / 20 %
Validierung**. Getunt wird nur auf den Trainingsdaten, und die Ausgabedatei wird
**nur geschrieben, wenn auch der Validierungsfehler besser wird** (sonst Exit-Code
4 und die kompilierten Defaults bleiben). Ohne diese Sperre hat der erste
Tuning-Versuch die Bewertung überfitet und die Engine messbar schwächer gemacht.


```text
setoption name Bot value v1200-thunder      # Elo + Stil + Handicap in einem Schritt
setoption name UCI_Elo value 1250         # freie Zahl erlaubt (700..3000)
setoption name UCI_LimitStrength value true
setoption name Style value Fire
setoption name Boldness value 45          # 10 = fortress ... 50 = thunder
setoption name Aggression.KingAttack value 260
setoption name Hash value 64              # Transpositionstabelle in MB
setoption name Threads value 4            # Lazy SMP (1..64, Standard 1)
setoption name WeightsFile value C:\mein-bot.txt
setoption name ReloadWeights
listbots
```

## Dokumentation

| Datei | Inhalt |
|---|---|
| `docs/NOTES.md` | Entscheidungsprotokoll und aktueller Stand |
| `docs/ROADMAP.md` | Meilensteine M1-M6 mit Status |
| `docs/WEIGHTS.md` | alle 90 Gewichte und wie man sie von außen setzt |
| `docs/BOTS.md` | Bot-Konzept, Stile, Elo-Profil, App-Integration |
| `docs/WEIGHTS.md` | alle 91 Gewichte und wie man sie von außen setzt |
| `ref/README.md` | Herkunft und Lizenz der Referenzquellen (PeSTO, Arasan) |
| `docs/APP_INTEGRATION.md` | **Einbindung in die eigene App** (Kotlin/C#) |
| `docs/BUILD_ANDROID.md` | Android-Build und Einbau in die App |

## Projektstruktur

```
src/            Engine (types, bitboard, position, movegen, eval, weights, botdata, search, selftest, uci)
data/           Gewichte, Stile, Elo-Profil, Bot-Katalog (generiert, optional zur Laufzeit)
dist/           fertige Android-Binaries fuer die App (virgin-arm64-v8a, -armeabi-v7a, -x86_64)
tools/          nightly.ps1 (Nachtlauf-Wrapper; Daten und Tests macht die Engine)
build_win.ps1   Windows-Build (MSVC 2022)
build_android.ps1  Android-Build (NDK 27, PIE oder -Static, -Dist kopiert nach dist\)
run_all.cmd     Ein-Befehl-Pipeline: Build, Daten, Selbsttest
run_match.cmd   Elo-Treppe gegen Stockfish 18 (1 s/Zug, 4 Partien je Stufe)
```

## Status

Meilensteine 1-5 sind fertig und verifiziert:

* **Suche**: iterative Vertiefung mit Aspiration Windows, PVS, Transpositionstabelle,
  Quiescence + SEE, Null-Move, LMR/LMP/Futility/Razoring, Killer/Countermove/History,
  `Contempt` für Remis-Bewertung. Nach 1.e4 findet sie `d7d5`.
* **Bewertung**: 13 Termgruppen (Material, PST, Mobilität, Königssicherheit,
  Bauernstruktur, Türme, Figuren, Bedrohungen, Raum, Initiative, Endspiel,
  Aggression, Komplexität) + Remis-Skalierung + Lazy Evaluation. Über `eval`
  ist die Aufschlüsselung abrufbar.
* **Bots**: Stil, Elo-Handicap (Tiefe, Nodes, Fehlerquote) und Wissens-Skalierung
  wirken; v700 spielt Tiefe 2, v2800 Tiefe 10-12.
* `--selftest` mit **43 Prüfungen** PASSED (Perft, Zobrist, Symmetrie,
  Gewichte, Zug-Kodierung, Zuglisten-Replay (Rochade/En passant), Bot-Katalog,
  SEE, Taktik, 8 Eval-Termtests).

Offen ist vor allem das **Texel-Tuning** der Termgewichte (die Startwerte sind
geschätzt, daher noch Züge wie `1.g4` im Selfplay), ein Eröffnungsbuch und die
Elo-Kalibrierung - siehe `docs/ROADMAP.md`.

## Lizenz

Eigener Code, MIT. Es wurde kein Quellcode aus GPL-Engines übernommen; nur
veröffentlichte Algorithmen und Ideen (Magic Bitboards, PVS, Texel-Tuning,
MadChess-Muster für Limit-Strength).

## Download

[Virgin: aktuelle Version herunterladen](https://github.com/parlue/virgin/releases/latest)

Downloads werden als GitHub-Release mit SHA-256-Pruefsumme bereitgestellt.
