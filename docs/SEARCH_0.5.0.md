# Virgin 0.5.0 – Suchalgorithmen, 31.10.2026

Ziel bleibt ein klarer Sieg gegen Carballo bei 1 s/Zug. Diese Version ist ein neuer Kandidat. Ein Elo-Zuwachs oder das Erreichen von 2200 Elo ist nicht nachgewiesen. Keine Partien gegen andere Engines wurden gestartet; die übernimmt der Nutzer. Die Positionsproben sind Windows-Messungen, kein Android-Leistungsnachweis.

## Änderungen

- Wurzelsuche mit PVS und korrekt begrenzten Nachsuchen. Bei Überschreitung des Aspirationsfensters exponentielle Erweiterung und bevorzugte erneute Prüfung des widerlegenden Zuges. Erst konvergierte Ergebnisse werden übernommen. Schwache Persönlichkeiten mit Zugrauschen erhalten weiterhin echte vollständige Wurzelbewertungen für ihre Auswahl.
- SEE verwendet dieselben Materialwerte wie die Bewertung. Dynamische Belegung, absolute Fesselungen, legale Königsschläge, en passant und Umwandlungsdifferenz werden berücksichtigt. Negative Schlagbilanz verwirft ein zwingendes Schachgebot nicht mehr pauschal.
- Begrenzte ruhige Schachgebote am ersten PV-Blatt der Schlagfolgensuche. Bauernangriffe auf Figuren und vorgerückte Freibauern erhalten Schutz vor vorschnellem Abschneiden; bei späten Zügen bleibt eine mildere Reduktion bestehen.
- Stellungsspeicher mit vier Einträgen pro Bucket, weiterhin 16 Bytes pro Eintrag und unverändertem Hash-Budget. Alterung je Wurzelsuche, geschützte tiefere Einträge und Wiederverwendung der Schlagfolgensuche. Gewichts-/Stilwechsel löschen gecachte Bewertungen.
- Kompakte Historie für den vorherigen und aktuellen Zug (4 KiB pro Thread); ergänzt die vorhandene History und beeinflusst Reihenfolge und Reduktion. Atomare Knotenzählung in Paketen von 256 statt bei jedem Knoten. Zeitgrenze wird dabei häufiger kontrolliert. Weitere ca. 4 KiB pro Thread für eine echte, auf 16 Halbzüge begrenzte Hauptvariante.
- UCI zeigt die tatsächlich durchsuchte Hauptvariante. TT-Abkürzungen können diese verkürzen. Debug-Befehl `see <uci>` gibt die Schlagbilanz aus.

Bewertungsgewichte und Persönlichkeitskatalog wurden für 0.5.0 nicht neu abgestimmt. Angriffsstil und plattformunabhängiger C++-Kern bleiben erhalten. Keine NNUE-Dateien, Trainingsnetze oder zusätzlichen Laufzeitdaten nötig. Die Python-Werkzeuge benötigen python-chess ausschließlich für die Entwicklungsprüfung.

## Prüfung

- 101 interne Windows-Prüfungen bestanden: Perft, Brettzustand, Wiederholungen, Matt/Patt, Taktik, Thread-Speicher, Bucket-Ersetzung und Nachspielen beider Android-Verluste bis zum Matt.
- Unabhängiges python-chess-Orakel: 319 legale Schlag-/Rückschlagfolgen, 0 Unterschiede. Das prüft SEE, keine vollständige taktische Lösung beliebiger Stellungen.
- Schmale Aspiration/PVS gegen vollständige Wurzelfenster bei ausgeschalteten selektiven Kürzungen auf zwei Endspielstellungen geprüft; identische Bewertung und legale Hauptvarianten.
- UCI-Prozesstest: ein/vier Threads, Optionswechsel während Suche, 100-ms-Zeitgrenze, reguläres Ende bestanden.
- Positionsaudits mit festen 200.000 Knoten sowie 1 s, 32 MiB Hash, ohne Spielstärkebegrenzung. Gemeldete Varianten unabhängig legal nachgespielt. JSON-Protokolle liegen hier. `nodes` und `depth` beziehen sich auf die letzte vollständig gemeldete Iteration, nicht zwingend auf die ganze erlaubte Knotenzahl.
- Android: arm64-v8a, armeabi-v7a und x86_64 mit NDK 27/API 21 gebaut; ELF-Architektur und PIE geprüft. Nur Android-Systembibliotheken libc/libm/libdl. Kein angeschlossenes Gerät, daher keine Ausführung auf Android geprüft.

## Vergleich und Grenzen

Die einzeln aufbewahrten Varianten root/core/balanced/cache/final erlauben die Änderungen nachzuvollziehen. Die breite erste Erweiterung ruhiger taktischer Züge kostete zu viel Suchtiefe und wurde eingeschränkt. Die finalen Werte unten vergleichen die neue Kandidatenfassung mit 0.4.0 auf identischen Stellungen aus den Verlustpartien. Ein einzelner Lauf je Fall, ein Thread, 1 s; Laufzeiten und Suchtiefe sind hardwareabhängig. Median der Tiefendifferenz über 22 gemeinsame Fälle: +0. Unterschiedliche Suchbäume und Kürzungen machen Tiefe allein nicht zum Spielstärkenachweis.

| Stellung (Halbzüge gespielt) | Stil | Tiefe 0.4 | Tiefe 0.5 | Zug 0.4 | Zug 0.5 |
|---|---|---:|---:|---|---|
| loss1-ply24 | Ausgeglichen | 14 | 12 | g4 | g4 |
| loss1-ply28 | Ausgeglichen | 11 | 12 | f4 | f4 |
| loss1-ply34 | Ausgeglichen | 11 | 10 | Rac1 | Kg2 |
| loss1-ply62 | Ausgeglichen | 19 | 18 | Rxc4 | Rxc4 |
| loss1-ply94 | Ausgeglichen | 21 | 20 | Ke3 | Ke3 |
| loss1-ply98 | Ausgeglichen | 23 | 23 | Kc3 | Kc3 |
| loss2-ply7 | Ausgeglichen | 12 | 11 | b5 | b5 |
| loss2-ply33 | Ausgeglichen | 13 | 13 | Qc5 | Qc5 |
| loss2-ply43 | Ausgeglichen | 14 | 14 | Qxg6 | Qxg6 |
| loss2-ply99 | Ausgeglichen | 15 | 14 | Rxc3 | Rxc3 |
| loss2-ply105 | Ausgeglichen | 17 | 17 | cxb4 | cxb4 |
| loss1-ply24 | Angriffslustig | 13 | 12 | g4 | f4 |
| loss1-ply28 | Angriffslustig | 11 | 12 | f4 | Rd4 |
| loss1-ply34 | Angriffslustig | 11 | 12 | Rd3 | Rd3 |
| loss1-ply62 | Angriffslustig | 17 | 17 | Rxc4 | Rxc4 |
| loss1-ply94 | Angriffslustig | 21 | 20 | Ke3 | Ke3 |
| loss1-ply98 | Angriffslustig | 23 | 23 | Kc3 | Kc3 |
| loss2-ply7 | Angriffslustig | 13 | 11 | Nf6 | b5 |
| loss2-ply33 | Angriffslustig | 12 | 12 | c6 | Qd6 |
| loss2-ply43 | Angriffslustig | 15 | 15 | Qxg6 | Qxg6 |
| loss2-ply99 | Angriffslustig | 15 | 15 | Rxc3 | Rxc3 |
| loss2-ply105 | Angriffslustig | 17 | 17 | cxb4 | cxb4 |

Nicht alle kritischen Entscheidungen ändern sich. Insbesondere der schon eingesperrte Läufer in Partie 2 ist nach 17.f5 nicht durch eine lokale Bewertungskorrektur zu retten. Weitere Partien müssen zeigen, ob die Suche solche Stellungen früher vermeidet. Die Entwicklung wurde nicht an erzwungenen Wunschzügen aus diesen zwei Partien festgemacht.

## Dateien und Verwendung

- `virgin-0.5.0.exe`: 416,768 Bytes (407.0 KiB).
- `virgin-arm64-v8a`: 813,208 Bytes (794.1 KiB).
- `virgin-armeabi-v7a`: 544,576 Bytes (531.8 KiB).
- `virgin-x86_64`: 844,216 Bytes (824.4 KiB).

Windows: `virgin-0.5.0.exe` als UCI-Engine in der selbst gewählten Schachoberfläche hinzufügen. Android: normalerweise `virgin-arm64-v8a` importieren; bei älteren 32-Bit-Geräten `virgin-armeabi-v7a`, bei x86-Geräten/Emulatoren `virgin-x86_64`. Es handelt sich um UCI-Binärdateien, keine APK.

Für den Stärketest `UCI_LimitStrength=false`, 1 s/Zug, gleiches Hash-/Thread-Budget und denselben Stil wie beim Vergleich verwenden. Die im Bot-Katalog benannten Elo-Stufen sind Einstellungen, keine gemessene Elo dieser Version. Vorhandene 0.4.0 und `dist/meine-engine` bleiben erhalten; die laufende 0.4-Testserie kann getrennt abgeschlossen werden. Auf dem Telefon wurde nichts installiert; der Nutzer entscheidet über die Oberfläche und den Import.

Sicherung vor der Arbeit: `backups/before-0.5-20261031-133101`. Versuchsstände: `experiments/search-0.5-20261031`. Prüfsummen der vier Binärdateien stehen in `manifest.json`.
