# Virgin 0.8.0 – 31.10.2026

Autor: Dirk D. Sommerfeld. Windows: virgin-0.8.0.exe. Android-UCI-Programme: virgin-arm64-v8a, virgin-armeabi-v7a, virgin-x86_64. In der Schach-App als neue Engine importieren. Die bisherigen 0.7.1-Dateien und BearChess-Einträge bleiben erhalten.

## Änderungen

1. **Materialarme Endspiele:** Turm und Leichtfigur gegen Turm, Turm gegen einzelne Leichtfigur und zwei Springer gegen den bloßen König bekommen eine Bewertung entsprechend ihrer begrenzten Gewinnchancen. Die Faktoren sind eigene UCI-Gewichte: Drawish.RookMinorVsRook=12, Drawish.RookVsMinor=20, Drawish.TwoKnights=8 (100 bedeutet keine Abschwächung). Es sind Heuristiken, keine Tablebase-Ergebnisse. Konkrete Matts und Materialgewinne werden weiterhin gesucht. Läufer und Springer gegen König sowie Turm gegen König behalten ihre Gewinnbewertung.
2. **Falscher Läufer mit Randbauern:** Gegen einen bloßen König direkt in der Umwandlungsecke wird die bekannte Festung erkannt, wenn sämtliche Bauern auf derselben Randlinie stehen und der Läufer die Umwandlungsecke nicht beherrscht. Bei zusätzlichen Figuren, anderen Bauernlinien oder passendem Läufer greift diese Regel nicht.
3. **Zugzwang:** Ohne Damen, mit höchstens drei Türmen/Leichtfiguren insgesamt, maximal sechs Bauern und höchstens einer eigenen Nichtbauernfigur wird vorsichtiger gesucht. Nullzug, Reverse-Futility, Razoring, interne Tiefenreduktion, späte stille Zugauslassungen und Late-Move-Reduktionen werden für diese Seite ausgesetzt. Das kann in Endspielen mehr Rechenarbeit kosten, vermeidet aber das Vertrauen auf einen unmöglichen Aussetzzug oder eine zu oberflächliche Suche.
4. **Königssicherheit:** Nahe gegnerische Bauern vor dem König zählen bereits vor dem direkten Figurenangriff als Gefahr; blockierte Bauernstürme zählen schwächer. Neues Gewicht KingSafety.PawnStorm=-10. Bei gegnerischer Dame plus unterstützender Figur bleibt ein Teil der Königssicherheit auch nach Materialtausch wirksam.

Adaptive verwendet unverändert dieselbe geprüfte TalChooser-Auswahlregel. Die verbesserten Engine-Bewertungen können ihre Kandidaten und Entscheidungen verändern. Zeitverwaltung, automatische Zugabgabe und Autor bleiben erhalten. Alle Dateien funktionieren mit eingebauten Gewichten, ohne Netzdateien.

## Was die Prüfungen zeigen

127 Selbsttests sowie UCI-, Zeit-, Adaptive- und Bibliotheksprüfungen bestanden. TalChooser: 680 Vergleichsfälle ohne Abweichung. Die kompakte ESP32-Hostvariante besteht dieselben 127 Selbsttests. Die Android-Bibliothek wurde im Android-14-x86_64-Emulator einschließlich Adaptive und Schließen bei laufender Suche geprüft. ARM-Binaries sind gebaut, aber nicht auf einem physischen ARM-Gerät ausgeführt.

Zwölf Stellungen wurden bei einem Thread und 200.000 Knoten mit 0.7.1 verglichen. In der kritischen Endstellung aus Partie 4 sinkt die Suchbewertung von +5,30 auf +0,63 für Schwarz. In mehreren Mittelspielstellungen ändern sich die bevorzugten Züge; die Partie-1-Züge 40.Se3 und 47.Dd3 bleiben bei diesem Budget bestehen. Nicht alle beobachteten Schwächen sind damit gelöst. Die gespeicherten Laufzeiten stammen teils aus parallelen Builds und sind kein Geschwindigkeitsvergleich.

Keine Vergleichspartien gegen andere Engines durchgeführt. Ein Elo-Zuwachs oder das Entwicklungsziel 3000 ist durch diese Prüfungen noch nicht nachgewiesen. Die nächste Aussage zur Spielstärke muss aus deinen Partien kommen.

Weitere Ausgaben: dist/virgin-android-library-0.8.0-20261031 mit libvirgin.so und AAR; dist/virgin-esp32s3-0.8.0-20261031 mit drei Firmwarevarianten. ESP32-Firmware noch nicht auf Hardware getestet.

Fachlicher Abgleich zu speziellen Endspielen: [Stockfish-Endspielreferenz](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_16/src/endgame.cpp). Virgins Änderungen verwenden eigene kleine Material- und Geometrieregeln.

## Zusätzliche Browser-Ausgabe – 04.11.2026

Unter `dist/virgin-web-0.8.0-20261104`: `virgin.wasm`, der zugehörige
Emscripten-Lader `virgin.js` und `engine-worker.js` als UCI-Adapter.
Adaptive, Elo-Bots, alle fünf englischen Stile und bis zu vier Suchthreads.
Die Browser-Ausgabe nutzt die vorhandene Bibliotheksschnittstelle und läuft
mit eingebauten Gewichten ohne externe Netzdateien.

Die Web-App benötigt Cross-Origin-Isolation (COOP/COEP), damit SharedArrayBuffer
und WebAssembly-Threads verfügbar sind. Ein lokaler Server, Integrationsbeispiel,
Quellpaket und `build_web.py` liegen bei. Details: [Web-Anleitung](../web/README.md).
Die Windows-, Android- und Core2-Ausgaben bleiben eigenständige Buildziele.
15 gezielte Chrome-Integrationstests einschließlich laufender Analyse/Stop,
Adaptive, Elo-Bot und Umwandlung bestanden.
