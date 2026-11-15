# Virgin 0.8.2 – Zeitverwaltung und Eröffnungsvariation

Autor: Dirk D. Sommerfeld. Stand: 04.11.2026.

## Spielzeit und Verhalten ohne Uhr

Der gemeinsame Suchkern verarbeitet weiterhin UCI-Restzeiten, Fischer-Inkrement
und Züge bis zur Zeitkontrolle. Beispiele für die Startstellung:

| Vorgabe | UCI-Beispiel |
|---|---|
| 5 Minuten je Seite | `go wtime 300000 btime 300000` |
| 5+2 | `go wtime 300000 btime 300000 winc 2000 binc 2000` |
| 10+10 | `go wtime 600000 btime 600000 winc 10000 binc 10000` |
| 40 Züge in 90 Minuten | `go wtime 5400000 btime 5400000 movestogo 40` |
| Höchstens 3 Sekunden pro Zug | `go movetime 3000` |
| Ohne Partieuhr | `go` |
| Daueranalyse bis Stop | `go infinite`, später `stop` |

Die GUI führt die Uhren und übermittelt nach jedem Zug die aktuellen Werte.
Bei einer weiteren Turnier-Zeitkontrolle schreibt sie Restzeit und movestogo
entsprechend fort. Die Engine selbst verwaltet keine komplette Partie-Uhr.

Ohne Zeitvorgabe darf Virgin normalerweise bis zu 5 Sekunden nutzen. Mit
Adaptive sind bis zu 10 Sekunden vorgesehen, um die fünf Kandidaten gründlicher
zu vergleichen. Das sind Obergrenzen und keine künstlichen Wartezeiten. Feste
Zeitvorgaben haben Vorrang; bei `go movetime 3000` gelten auch in Adaptive
höchstens 3 Sekunden. Explizite Tiefen-/Knotenvorgaben bekommen keine versteckte
5-/10-Sekunden-Grenze. Daueranalyse wartet weiterhin auf Stop.

Ein beständiger bester Zug und eine stabile Bewertung erlauben frühere
Zugabgabe. Wechselnde Kandidaten und Bewertungseinbrüche können mehr des
verfügbaren Budgets beanspruchen. Die harte Grenze wird dabei nie verlängert.
Die Kosten der letzten abgeschlossenen Suchiteration helfen abzuschätzen, ob
noch eine weitere Iteration hineinpasst. Erzwungene Züge werden schnell abgegeben.

Die schnelle Eröffnungs-Uhrensuche bleibt: in den ersten vier vollständigen
Zügen höchstens 1,5 Sekunden Grund-Zielzeit und 3 Sekunden Höchstzeit. Diese
Sonderregel betrifft Partieuhren, nicht ausdrücklich vorgegebene Denkzeiten.

## Zeitpuffer

Neue UCI-Option `Move Overhead`: 0–5000 ms, standardmäßig 50 ms auf
Windows/Android/Web und 100 ms auf ESP. Sie lässt Zeit für Ausgabe und
Übertragung und bleibt auch mit Adaptive einstellbar. Bei festen Denkzeiten
wird der Puffer ebenfalls vor dem Rechnen abgezogen. Es handelt sich um eine
kooperative Zeitgrenze, nicht um eine harte Echtzeit-Garantie des Betriebssystems.
Leere/negative explizite Restzeiten werden als Zeitnot behandelt; Nullwerte für
movetime und nodes führen nicht versehentlich zu unbegrenzter Suche.

ESP prüft die Grenzen häufiger, ohne den bisherigen Yield-/Watchdog-Rhythmus
entsprechend zu vervielfachen. Speicher- und Suchpfadgrenzen bleiben unverändert.

## Kleine, sichere Eröffnungsvariation

Kein Eröffnungsbuch und keine neue große Datentabelle. In den ersten acht
Halbzügen können zeitgesteuerte Partiezüge zwischen nahezu gleichwertigen
Kandidaten variieren. Dazu werden drei Kandidaten verglichen, in Adaptive die
bereits vorhandenen fünf. Voraussetzungen für eine Alternative:

- Mindestens Suchtiefe 6 und zwei aufeinanderfolgende vollständige Kandidatenrunden.
- Höchstens 10 Centipawns Abstand zum Besten in beiden Runden.
- Bewertung des Kandidaten zwischen diesen Runden um höchstens 25 Centipawns verändert.
- Keine Mattbewertung, kein Schachgebot gegen den eigenen König und keine bereits deutlich schlechte Stellung (unter −75 cp).
- Eine nichtnegative Bestbewertung wird nicht gegen eine negative Alternative getauscht.
- Legale Hauptvariante mit mindestens vier Halbzügen; während der ersten bis zu sechs Halbzüge kein Netto-Materialverlust nach vollständigen Zugpaaren.

Bei unzureichender Prüfung bleibt der beste gefundene Zug. Die Suche kann
keine objektive Verlustfreiheit beweisen; die Grenzen dienen als konservative
Filter. Zufall greift ausschließlich innerhalb der zugelassenen Auswahl.
Explizite Tiefen-/Knotenanalyse und Daueranalyse verwenden diese Variation nicht.
Bestehende Elo-Handicaps behalten ihren bisherigen Such-/Zufallspfad.

Adaptive selbst behält seine geprüfte Auswahlregel. Nur im frühen Eröffnungsfall
kann die zusätzliche enge Auswahl zwischen gleichwertigen Zügen greifen; die
Spielerbewertung und der Folgezustand werden dann für den wirklich gespielten
Zug aktualisiert. Die spätere Adaptive-Logik bleibt unverändert.

## Ausgaben und Abgrenzung

Windows-UCI, Android-UCI/OEX, Android-Bibliothek/AAR, WebAssembly und die drei
ESP32-S3-Profile werden separat gebaut. Die bisher veröffentlichten Versionen
bleiben erhalten. M5Chess/Core2 wird nicht weiterentwickelt, nicht neu gebaut
und nicht geflasht; seine bisherige Zeit-/Auswahllogik ist im gemeinsamen
Code ausdrücklich ausgenommen.

Build- und Prüfprotokolle liegen unter `reports/*0.8.2*`. Die Release-Manifeste
nennen die tatsächlich gebauten Dateien und die ausgeführten Prüfungen.

## Dateien verwenden und selbst bauen

- Windows: `windows/virgin-0.8.2.exe` als UCI-Engine in die Schach-GUI eintragen.
- Android: `android/Virgin-0.8.2-OEX.apk` installiert den Engine-Anbieter für OEX-kompatible GUIs. Es ist keine vollständige Schach-App. Für eigene Apps stehen `android/virgin-0.8.2.aar`, `jniLibs/`, Java-Wrapper und C-API-Header bereit.
- Web: `web/` enthält WASM, Lader, Worker und einen lokalen Server; COOP/COEP-Header bleiben erforderlich.
- ESP32-S3: `esp32-s3/` enthält die bisherigen generischen UCI-Profile ohne PSRAM, mit QSPI-PSRAM und mit OPI-PSRAM. Das sind keine neuen Touch-AMOLED-/Bauerino-Oberflächen. Das Speicherprofil muss zur Hardware passen.

Das Quellpaket enthält den gemeinsamen Kern und die Buildziele. Windows benötigt
CMake 3.20+, Ninja und Visual Studio C++ Build Tools; `build_win.ps1 -Generator Ninja`
baut das Windows-Programm. Android benötigt zusätzlich Android SDK Build Tools 36,
Platform 36, NDK 27.0.12077973 und ein JDK, etwa das mit Android Studio mitgelieferte.

```sh
python tools/build_082_targets.py android
python build_web.py
python -m pip install platformio==6.2.0
python tools/build_082_targets.py esp
```

Für WebAssembly zuerst das offizielle Emscripten-SDK unter `tools/emsdk`
installieren und aktivieren; die verwendete Version steht im Web-Manifest.
`requirements-tests.txt` aus dem Quellpaket ist nur für zusätzliche Python-UCI-Tests
nötig, nicht für den Engine-Betrieb. Die eingebauten Prüfungen laufen mit
`virgin --selftest`. Der ESP-Build verwendet nur die explizit benötigten
SDK-Komponenten; die USB-/VFS-Konsolenabhängigkeit ist ausdrücklich angegeben.

Keiner dieser Buildbefehle flasht automatisch ein Gerät. Für eine spätere
Übertragung der S3-UCI-Firmware kann das passende PlatformIO-Profil gezielt mit
`-t upload --upload-port PORT` gebaut/übertragen werden. Kein Core2-Image verwenden.
