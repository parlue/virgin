# Virgin 0.8.4 – Suchkorrektur und Kandidatenanalyse

Autor: Dirk D. Sommerfeld. Stand: 5. November 2026. Entwicklungskandidat auf Grundlage von 0.8.2; 0.8.3 wird damit nicht nachträglich als veröffentlichte Version behauptet.

## Was geändert wurde

- Ruhige Schachgebote am ersten PV-Blatt der Schlagfolgensuche bleiben erhalten, wenn Stand-pat die untere Suchgrenze bis auf einen Centipawn an die obere heranführt. Vorher wurde dieses ursprüngliche PV-Fenster fälschlich als Nullfenster behandelt. Ein eigens konstruierter Mattfall scheitert mit dem alten Ausdruck und besteht mit der Korrektur.
- Legale Rochaden werden als wichtige ruhige Züge vor Futility-/Late-Move-Pruning geschützt und bei Reduktionen berücksichtigt. Das ist keine pauschale Rochadepflicht; die Suche darf weiterhin einen besseren Zug wählen.
- Standard-UCI-Option `MultiPV` (1–5) und `go searchmoves` für gezielte Kandidatenvergleiche. Nur legale angegebene Züge werden untersucht, auch in Hilfsthreads. Einzelne eingeschränkte Kandidaten werden bis zur angeforderten Tiefe analysiert und nicht als sofort abzugebender Pflichtzug behandelt.
- Nachfolgende Grenzen hinter `searchmoves`, doppelte/illegale Einträge, leere Kandidatenlisten, Stop und Adaptive werden geprüft. Eine Liste ohne legale Kandidaten ergibt `bestmove 0000`, nicht eine unbeschränkte Ersatzsuche.
- Standard bleibt MultiPV=1. Adaptive verwendet seine bisherigen fünf internen Kandidaten; auch dort bleibt eine ausdrücklich angegebene Zugliste bindend. Eröffnungszufall ist bei expliziter Kandidatenanalyse ausgeschaltet.
- Keine neuen Bewertungsgewichte, kein neues Netz, keine Enginezug-Sonderfälle für bestimmte PGN-Stellungen. Die bestehenden Botprofile und Datenformate bleiben erhalten.

## Status der Stockfish-Schwächen

Die Damentausch- und Läuferopferfälle sind nicht allgemein gelöst. Ein gezielter MultiPV-Lauf mit je zwei Kandidaten und zwei Millionen Knoten bevorzugt bei 27.Weiß aus Partie 5 Qe4 gegenüber Qxc7; bei 15.Weiß aus Partie 9 bevorzugt Virgin Bxf4 weiterhin knapp gegenüber Bh2, entgegen Stockfish.

Im Vergleich auf zwölf Stellungen bei 350.000 Knoten blieb die Zugwahl in elf Fällen gleich. In Partie 9 wechselte sie von Bh2 zu dem schlechteren Bxf4; Stockfish schätzt diesen Unterschied auf rund 1,12 Bewertungseinheiten. Eine Suchkorrektur ist deshalb kein Nachweis, dass jede Zugentscheidung bei festem kleinem Budget besser wird. Rohdaten und Varianten stehen in `checks/final-positions.json` und `checks/final-candidates.json`.

Zwei größere erste Ansätze – allgemein geschützte Minor-Rückzüge und pauschal stärkere Bauernstrafen im Endspiel – wurden getrennt und kombiniert geprüft und wegen gemischter Ergebnisse verworfen. Ihre Messdaten liegen in `checks/ablation.json`; sie sind nicht Bestandteil der ausgelieferten Engine.

Ein kurzer A/B-Lauf gegen 0.8.2 mit acht Partien, vier Eröffnungspaaren, einem Thread, 32 MiB Hash und 0,1 s/Zug ergab **3:5 aus Sicht von 0.8.4** (zwei Siege, vier Niederlagen, zwei Remis; eines davon am 180-Halbzüge-Limit). Das ist ein kleiner Funktions-/Vergleichslauf, kein signifikanter Elo-Test und kein Nachweis eines Stärkegewinns. Für die Freigabe einer stärkeren Spielversion sind längere unabhängige A/B-Partien erforderlich.

## Ausgaben

- Windows x64: `windows/virgin-0.8.4.exe` als UCI-Engine; `virgin.dll` und `virgin_native.h` für die native Einbindung.
- Android: `android/Virgin-0.8.4-OEX.apk`, universell für arm64-v8a, armeabi-v7a, x86_64 und x86. Paketkennung und vorhandener Signaturschlüssel sind wie bei 0.8.2; versionCode ist 80401. Dazu `virgin-0.8.4.aar`, JNI-Bibliotheken, Java-Wrapper und einzelne UCI-Binärdateien. Die APK ist ein Engine-Anbieter, keine vollständige Schachoberfläche.
- Web: `web/` mit WASM, Loader und Worker. Lokal über `python serve.py` starten; die beiliegenden COOP/COEP-Header sind für Threads erforderlich. Keine Website wurde veröffentlicht.
- ESP32-S3: `esp32-s3/mini_no_psram`, `mini_psram_qspi`, `mini_psram_opi`. Dies sind S3-Ausgaben der Virgin-Engine, keine ESP32-C3-Gateway-Firmware. Keine Geräte wurden mit 0.8.4 geflasht. Core2 wurde weder gebaut noch aktualisiert.
- Quellen: `Virgin-0.8.4-source.zip`; Integritätsdaten: `manifest.json` und `SHA256SUMS.txt`.

## Prüfung und Grenzen

- Windows und kompakter 32-KiB-Host-Kern: jeweils 149 interne Prüfungen bestanden, einschließlich Perft, Brett-/Zobrist-Konsistenz, Matt/Patt, Wiederholung, Schlagfolgen, TT-Konkurrenz, Varianten und der neuen Suchregressionen.
- Isolierter Kontrollbuild: ausschließlich der neue Matt-Testfall und sein zusammenfassender Testgruppenstatus scheitern nach Rücksetzen des alten Fenster-Ausdrucks; alle anderen Prüfungen bestehen. Das bestätigt die konkrete Fehlerkorrektur.
- Neue UCI-Prüfung: neun Gruppen bestanden, darunter MultiPV/searchmoves mit einem/vier Threads, Einzelkandidat-Tiefe, nachfolgende Grenzen, illegale/doppelte Einträge, leere Liste, Adaptive und Stop. Bisherige Zeit-, Eröffnungs-, Handicap- und Stop-Prüfungen bestanden ebenfalls.
- Windows-DLL: acht Prüfgruppen bestanden, unter anderem identische Tiefe-6-Suche gegenüber der Standalone-Engine und zwölf Create/Search/Quit/Destroy-Zyklen.
- Web: 15 Integrationstests in einem isolierten Headless-Browser bestanden, einschließlich ein/vier Threads, Stop, Adaptive, Umwandlung und Dispose. Keine vorhandenen Browserfenster wurden bedient.
- Android: alle acht ELF-Dateien auf Zielarchitektur, PIE/Shared-Library-Typ und mindestens 16-KiB-LOAD-Ausrichtung geprüft. APK/AAR enthalten genau die gebauten Binärdateien. APK-Signatur und Ausrichtung verifiziert; Zertifikat stimmt mit 0.8.2 überein. Keine Android-Laufzeit oder physische ARM-Hardware in diesem Lauf verfügbar; keine App installiert.
- ESP32-S3: alle drei Profile gebaut, Versionskennung, Bild-Checksumme und Validierungshash geprüft. Kein Hardware-Laufzeittest.

## Kandidatenanalyse verwenden

Beispiel für Partie 5 vor 27.Weiß:

```text
setoption name Threads value 1
setoption name MultiPV value 2
position fen 2r1b1k1/1pq2ppp/p3p3/4Q3/P2P1N2/8/5PPP/4R1K1 w - - 1 27
go nodes 2000000 searchmoves e5e4 e5c7
```

MultiPV 1 wieder einstellen, wenn nur ein bester Zug für die Partie benötigt wird. Bei der Analyse kann die zuletzt vollständig gemeldete Iteration weniger Knoten/Zeit ausweisen als das gesamte Budget, wenn die nächste Iteration am Limit endet.

## Quellen bauen

Windows: `powershell -ExecutionPolicy Bypass -File build_win.ps1` oder `python tools/build_084_targets.py windows`.
Weitere Ziele: `python tools/build_084_targets.py android`, `web` bzw. `esp`.

Die Android-Werkzeuge benötigen das vorhandene SDK/NDK. Für APK-Upgrades denselben eigenen Schlüssel benutzen: optional `VIRGIN_SIGNING_DIR` setzen oder `android/oex/private` verwenden. Private Schlüssel sind nicht im Quellpaket enthalten. Web benötigt ein lokales Emscripten-SDK (`EMSDK` oder `tools/emsdk`). Die ESP-Profile verwenden das bestehende PlatformIO-Projekt. Die bisherigen Versionspakete werden nicht überschrieben.
