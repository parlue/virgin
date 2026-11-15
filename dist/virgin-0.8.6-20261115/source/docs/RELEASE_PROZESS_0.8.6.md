# Release-Prozess und Stärkearbeit (Stand 15. November 2026, Version 0.8.6)

Diese Datei hält fest, wie 0.8.6 entstanden ist, damit die nächste Version
(0.8.7 usw.) ohne Neuentdeckung gebaut werden kann.

## 1. Was in 0.8.6 geändert wurde (nur Suche, keine Eval)

Ausgangspunkt war der Eindruck, die Suchtiefe sei oft zu niedrig. Messung
(Ryzen 5 7535U, 1 Thread, 1 s/Zug, Ausgangsversion):

| Stellung | Tiefe in 1 s |
|---|---|
| Mittelspiel ruhig | 12-16 |
| Kiwipete (taktisch) | 10 |
| Turmendspiel mit Bauern | 8 |

Änderungen in `src/search.cpp`:

1. **Endspiel-Pruning**: Futility, LMP und LMR waren in jeder dünnen Stellung
   abgeschaltet (`zugzwangRisk`). Jetzt nur noch in reinen Bauernendspielen
   (`pawnEnding`). Null-Move, Razoring, Reverse Futility und IIR bleiben wie
   vorher an `endgameRisk` gebunden. Turmendspiel: Tiefe 8 -> 12 in 1 s.
2. **SEE-Pruning** in der Zugschleife (Tiefe <= 8, nicht PV, nicht im Schach,
   kein Schach gebender Zug, nicht erster Zug): Schlagzüge unter `-90*Tiefe`,
   ruhige Züge unter `-25*Tiefe^2` werden übersprungen.
3. **Capture History** (`g_captureHistory[Farbe][Figur][Zielfeld][Geschlagen]`,
   pro Thread, rund 28 KB): fließt als `/8` in `scoreMove` ein, Bonus für den
   Cutoff-Schlagzug, Malus für vorher probierte Schlagzüge.
4. **LMR-Formel**: Divisor 2,36 -> 1,75 (`initSearch`). 1,45 war im Blitz noch
   besser (+80), wurde aber wegen Überanpassung an kurze Bedenkzeit nicht
   gewählt.

Nicht angefasst: Adaptive-Modus (immer 5 Kandidaten pro Zug, das kostet
Tiefe, ist aber Absicht) und die ersten vier Züge (`openingVariety` schaltet
auf MultiPV 3).

`src/match.cpp`: Umgebungsvariable `VIRGIN_MATCH_SEED` = vier seeded Zufallszüge
nach der Eröffnung, nur wenn das Material ausgeglichen bleibt. Ohne Variable
unverändert. Ohne sie spielt `--match` nur 8 feste Eröffnungen deterministisch,
parallele Läufe wiederholen dann dieselben Partien.

## 2. Messergebnisse (Selbstvergleich, 1 Thread, je Seite gleiche Zeit)

| Schritt | Partien | Zeit/Zug | Ergebnis | Elo |
|---|---|---|---|---|
| Endspiel-Pruning vs. Ausgang | 200 | 100 ms | +115 =15 -70 | +80 +/- 47 |
| Endspiel-Pruning vs. Ausgang | 150 | 300 ms | +90 =18 -42 | +115 +/- 55 |
| + SEE-Pruning | 200 | 150 ms | +109 =19 -72 | +65 +/- 47 |
| + Capture History | 200 | 150 ms | +102 =22 -76 | +45 +/- 46 |
| a3 vs. Ausgang (Zwischenstand) | 200 | 200 ms | +117 =24 -59 | +104 +/- 47 |
| LMR-Divisor 2,05 vs. a3 | 200 | 150 ms | +124 =20 -56 | +123 +/- 48 |
| LMR-Divisor 1,75 vs. 2,05 | 200 | 150 ms | +123 =21 -56 | +121 +/- 48 |
| LMR-Divisor 1,45 vs. 1,75 | 200 | 150 ms | +113 =19 -68 | +80 +/- 47 |
| LMR 1,45 vs. a3 (ohne LMR-Änderung) | 120 | 400 ms | +70 =14 -36 | +101 +/- 61 |

Wichtig beim Lesen: Die Einzelwerte bei 150 ms summieren sich nicht. Bei
längerer Bedenkzeit (400 ms) war der LMR-Gewinn nur etwa +100 statt +300.
Im Blitz belohnt sich tieferes Beschneiden überproportional. Gegen andere
Engines und auf Handy/ESP32 wurde nichts gemessen. Die Fehlerbalken
(+/- 47 bei 200 Partien) sind groß, einzelne Schritte unter etwa 40 Elo sind
nicht belegbar.

## 3. A/B-Messung wiederholen

```
tools/ab_run.sh <neu.exe> <alt.exe> <tag> [partien je Prozess=40] [ms=150] [seedBasis=300]
```

* Beide Binaries liegen in `build/win/`. Das Skript startet 5 parallele
  `--match`-Prozesse (je 1 Thread, `VIRGIN_MATCH_SEED` verschieden), wartet bis
  alle fertig sind und druckt W/D/L und Elo mit 95 %-Fehlerbalken.
* 5 x 40 Partien bei 150 ms dauern etwa 5 Minuten, bei 400 ms deutlich länger.
* Seeds nicht wiederverwenden, sonst entstehen dieselben Eröffnungen.
* Jede Suchänderung zuerst gegen den direkten Vorgänger messen, dann ab und zu
  gegen eine ältere Referenz und bei längerer Bedenkzeit gegenprüfen.
* Der Selbsttest (`virgin.exe --selftest`) muss `RESULT: PASSED` liefern.
* Achtung: Bei Prozess-Wartescripten in Git Bash taugt `pgrep` nicht für
  Windows-Prozesse; `tasklist | grep` verwenden.
* Gesicherte Binaries dieser Arbeit: `virgin_ref0.exe` (Ausgang 0.8.5),
  `virgin_a1.exe` (Endspiel-Pruning), `virgin_a2.exe` (+SEE), `virgin_a3.exe`
  (+Capture History), `virgin_c1/c2/c3.exe` (LMR 2,05 / 1,75 / 1,45).
  `virgin_base.exe` wurde dabei überschrieben (jetzt Stand a1).

## 4. Auslieferung bauen (alle Plattformen)

Versionsstellen (bei jeder neuen Version ersetzen): `src/version.h`,
`android/oex/build.py`, `build_web.py` sowie die Skripte unten. 0.8.6 wurde
durch `sed` aus den 0.8.5-Skripten erzeugt (`0.8.5`->`0.8.6`,
`20261107`->`20261115`, `085`->`086`):

* `build086.cmd`, `build086extra.cmd` (Windows exe, DLL, kompakter Kern)
* `tools/build_086_targets.py` (Hauptskript, Aufruf mit `windows`, `web`,
  `android`, `esp`)
* `tools/package_android_086.py` (AAR), `tools/build_linux_086.py`,
  `tools/verify_linux_086.py`, `tools/test_086_uci.py`,
  `tools/test_086_performance.py`, `tools/virgin_086_test.py`

Zielordner: `dist/virgin-0.8.6-20261115/` (Unterordner `windows`, `android`,
`web`, `esp32-s3`, `linux`, `source`, `checks`).

Aufruf (nacheinander, nicht parallel, sonst konkurrieren die Builds um CPU):

```
python tools/build_086_targets.py windows
python tools/build_086_targets.py web
python tools/build_086_targets.py android
python tools/build_086_targets.py esp
```

Werkzeuge und Fallstricke:

* **ESP32-S3**: PlatformIO. In dieser Python-Installation fehlt das Modul;
  `build_*_targets.py` ruft deshalb `~/.platformio/penv/Scripts/pio.exe` direkt
  auf (nicht `python -m platformio`). Drei Varianten: `mini_no_psram`,
  `mini_psram_qspi`, `mini_psram_opi`. Der ESP-Kern nutzt `VIRGIN_EMBEDDED`
  bzw. `VIRGIN_CORE2` und hat kleinere Stacks (`MAX_PLY`); die neue Capture
  History braucht etwa 28 KB zusätzlichen RAM - bei den PSRAM-losen Varianten
  prüfen, ob der Build noch passt und ob er läuft (nicht auf Hardware getestet).
* **Android**: NDK 27.0.12077973 unter `%LOCALAPPDATA%\Android\Sdk\ndk`,
  vier ABIs (arm64-v8a, armeabi-v7a, x86_64, x86), jeweils UCI-Binary und
  `libvirgin.so`, danach OEX-APK und AAR (JDK aus Android Studio `jbr`).
* **Web**: Emscripten aus `tools/emsdk`, `build_web.py`.
* **Linux** (statisch, x86_64, ARMv6, ARMv7): Zig 0.15.2, in
  `build/linux-toolchain/zig-x86_64-linux-0.15.2/zig` (Linux-Binary, läuft in
  WSL `Ubuntu-24.04`). Der Ordner war aufgeräumt worden und wurde am
  15.11.2026 neu geholt: `ziglang.org/download/index.json`, Eintrag
  `0.15.2 / x86_64-linux`, SHA-256
  `02aa270f183da276e5b5920b1dac44a63f1a49e55050ebde3aecc9eb82f93239`
  (geprüft). Quelle des Download-Skripts: `get_zig085.py` im Temp-Ordner,
  nicht im Projekt - bei Bedarf neu schreiben.
* PowerShell-Skripte (`build_win.ps1`) laufen nur über das PowerShell-Werkzeug,
  nicht über `powershell -Command` aus Bash (Ausführungsrichtlinie).
* Der alte Core2-Zweig (`esp32_core2`) ist ein eigener Stand und war auch bei
  0.8.5 kein Ziel.

Prüfen (alle mit absolutem Pfad aufrufen, sonst findet `Engine` die Datei nicht):

```
<exe> --selftest                                  # Windows und build/086-compact/virgin.exe
python tools/test_086_uci.py <exe>
python tools/test_086_performance.py <exe>
wsl -d Ubuntu-24.04 -- python3 tools/verify_linux_086.py   # Linux x86_64 + ELF-Prüfung aller drei
tools/emsdk/node/*/node.exe tools/test_web.cjs <dist/.../web>   # Chrome headless
python tools/assemble_release_086.py              # Doku, Quellstand, checks, manifest.json, SHA256SUMS.txt
```

`test_web.cjs` hatte die Version fest eingetragen und wurde auf 0.8.6 gezogen;
bei der nächsten Version wieder anpassen. `assemble_release_086.py` erzeugt aus den
0.8.5-Vorlagen (Versionsersetzung) README, Lizenz, EINBINDEN.md, Quellstand,
manifest und Prüfsummen; Release-Text in `docs/RELEASE_<Version>.md` zuerst
schreiben.

Nach dem Bauen: Prüfberichte nach `checks/`, `SHA256SUMS.txt`,
`manifest.json`, `README.md` und `RELEASE_<Version>.md` in den Zielordner
legen (Vorlage: `dist/virgin-0.8.5-20261107/`). Hardware-Laufzeittests
(Android-Gerät, ARM, ESP32) wurden bisher nicht gemacht und müssen im
Release-Text als ungetestet stehen.

## 5. Offene Ideen für spätere Schritte

* Suchparameter systematisch tunen (Futility/Razoring-Margins, Null-Move-R,
  Aspiration, LMP-Tabelle); jede Variante per `tools/ab_run.sh`.
* Singular Extensions.
* Königssicherheit mit gewichteten Angreifern, Phasenskalierung nach Material,
  größerer Texel-Datensatz (siehe `docs/ROADMAP.md` M7).
* Make/Unmake lohnt wohl nur nach einem Profiler-Lauf: die Engine macht
  300-650 k Nodes/s (1 Thread) wegen der schweren Eval, nicht wegen Copy-Make.
* Adaptive-Modus: 5 Kandidaten pro Zug kosten Tiefe; weniger Kandidaten oder
  eine zweistufige Suche prüfen.
