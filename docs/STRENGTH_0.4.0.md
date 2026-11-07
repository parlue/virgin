# Virgin 0.4.0 – Wiederholung und gefesselte Verteidiger

Stand: 31.10.2026. Ausgangspunkt ist die zweite vom Nutzer gelieferte Partie von Virgin 0.3.0 gegen das Teufelchen, weiterhin bei 1 Sekunde pro Zug. Ziel bleibt ein klarer Sieg gegen Carballo; ein Elo-Gewinn ist auch mit dieser Version noch nicht nachgewiesen. Keine Matches gegen andere Engines wurden gestartet.

## Zwei notwendige Korrekturen zur Partieauswertung

**Die Qualität wurde in dieser Partie nicht von Virgin hergegeben.** Weiß und Schwarz behalten bis zum Schluss beide Türme. 20...Txf5 schlägt den weißen Springer; Weiß schlägt anschließend den schwarzen Läufer auf b4. Nach 23...Txh4 und am Schluss hat Weiß zwei Bauern weniger, bei gleichem Figurenbestand. Der Königsdruck ist ein zusätzliches Stellungsproblem. Unterschiedliche Engine-Bewertungen in Bauerneinheiten allein sind kein Beweis für einen Fehler; entscheidend sind konkrete Varianten und die Ergebnisse vieler Partien.

**Die gelieferte PGN enthält nur zwei Vorkommen der Schlussstellung**, nach den Halbzügen 59 und 63. Auch keine andere Stellung tritt darin dreimal auf. Der Nutzer bestätigt, dass die Oberfläche automatisch beendet hat. Bei vollständiger PGN wurde damit bereits nach zweifacher Wiederholung abgebrochen; die Abschlussregel der Oberfläche sollte geprüft werden. Die Oberfläche wurde hier nicht verändert. Virgins UCI-Prozess beendet selbst keine Partie durch eine Remiserklärung. Die Suchbewertung und die Remisentscheidung der Oberfläche sind getrennt zu betrachten.

## Nachgewiesener Suchfehler und Korrektur

Die bisherige Suche behandelte jedes zweite Vorkommen eines Stellungsschlüssels als Remis. Das ist als Abbruchheuristik für einen Zyklus **innerhalb der gerade untersuchten Suchvariante** üblich, war hier aber pauschal auch auf früher gespielte Partiestellungen angewendet. Damit konnte ein Zug, der zum ersten Mal eine frühere Partiestellung wiederherstellt, sofort 0,00 erhalten, obwohl der Gegner beim nächsten Mal noch abweichen kann.

0.4.0 merkt sich die Grenze zwischen Partieverlauf und aktueller Suche:

- Wiederholungen ausschließlich im bisherigen Partieverlauf benötigen zwei frühere Vorkommen, also drei einschließlich der aktuellen Stellung.
- Ein Zyklus zurück zur Suchwurzel oder innerhalb der aktuellen Suchvariante kann weiterhin unmittelbar beendet werden.
- Die Anfangsstellung aus `position startpos` bzw. `position fen` wird jetzt ebenfalls in der UCI-Partiehistorie gespeichert.
- Die bestehende Grenze nach künstlichen Nullzügen bleibt erhalten.

Die Regressionstests bauen echte reversible Springerzyklen auf und prüfen historische Zweifach-/Dreifachwiederholung, Zyklen innerhalb der Suche sowie Nullzuggrenzen getrennt.

## Gefesselte Verteidiger

Die Bewertung unterschied zuvor nicht ausreichend zwischen geometrisch angegriffenen Feldern und tatsächlich möglichen Rückschlägen. Beispiel: Weißer König g1, weißer Bauer g2, schwarze Dame g6 und schwarzer Läufer h3. Der Bauer greift geometrisch h3 an, darf den Läufer aber nicht schlagen, weil er dann die g-Linie gegen den eigenen König freigibt.

0.4.0 berechnet absolute Fesselungen für beide Seiten. Verwendbare Bauernangriffe, Figurenmobilität, Doppelangriffe, hängende Figuren und die Abschätzung sicherer Schachfelder berücksichtigen die Fesselungslinie. Ein gefesselter Turm darf sich weiter entlang seiner Linie bewegen; ein diagonal gefesselter Bauer darf den fesselnden Läufer schlagen, wenn der Zug auf dieser Linie bleibt.

Die geometrische Angriffskarte wird separat beibehalten: Gefesselte Figuren schließen weiterhin bestimmte gegnerische Königszüge aus. Die tatsächliche Legalitätsprüfung wurde nicht durch die Bewertungsheuristik ersetzt. Dazu passt die Unterscheidung in [FIDE-Regel 3.1.3, hier im archivierten Regeltext 2018](https://handbook.fide.com/chapter/E012018). Neue Tests prüfen beide Sichtweisen ausdrücklich.

Es wurden diesmal **keine weiteren Bewertungsgewichte oder Stilprofile verändert**. Die 91 Gewichtsnamen und alle Stile bleiben erhalten. Die Änderungen sind allgemein; kein Zug und keine Bewertung wurde für diese Partie fest vorgegeben.

## Stellungstests und offene Schwächen

Vor/nach dem Umbau wurden die kritischen Stellungen mit frischem Hash, voller Stärke, jeweils 1 Sekunde, einem bzw. vier Threads und beiden Stilen Ausgeglichen/Angriffslustig untersucht. Alle JSON-Protokolle liegen bei. Mehrkern- und zeitbegrenzte Suchen sind nicht deterministisch.

Die deutlichste Veränderung betrifft die Schlussstellung **vor 32.Dd5**:

| Ein Thread, 1 Sekunde | Virgin 0.3.0 | Virgin 0.4.0 |
|---|---:|---:|
| Ausgeglichen | 0,00 | −0,92 |
| Angriffslustig | 0,00 | −1,05 |

In allen vier Fällen bleibt der gewählte Zug Dd5. Der Fortschritt ist hier die beseitigte sofortige Remisannahme, kein nachgewiesener besserer Rettungszug. Auch die neue Suche kann später noch tatsächlich erreichbare Wiederholungen finden; das ist erwünscht.

Die nachgestellten Suchen wählen vor Zug 14 nicht durchgehend das gespielte Sc3, auch bereits 0.3.0 nicht. Die gesamte Partiezugfolge lässt sich deshalb nicht als fest reproduzierbarer Fehler unter frischem Hash darstellen. Tatsächlich benutzte App-Optionen, Verlauf der Hashtabelle und Bedenkzeitbedingungen können die Auswahl verändern. Vor Zug 21 wählen beide Versionen weiterhin h4. Die Königsflügelprobleme sind **nicht vollständig gelöst**, und eine bessere Gesamtspielstärke ist nicht aus den einzelnen Zugänderungen abzuleiten.

Zusätzlich wurden kritische Stellungen der ersten Partie mit 0.4.0 kontrolliert. Die Zugwahl bleibt wechselhaft; insbesondere wird keine pauschale Aussage übernommen, dass De3/h4/g3 mit jeder neuen Version stets vermieden würden.

Ein kurzer Einthread-Benchmark mit Tiefe 7 lieferte zuletzt rund 0,82 Mio. Knoten/s für 0.4.0 und 0,77 Mio. für 0.3.0. Das sind einzelne Messungen mit unterschiedlichen Suchbäumen, kein belastbarer allgemeiner Geschwindigkeits- oder Elo-Nachweis. Die Fesselungsberechnung benötigt etwas zusätzlichen Stack je Bewertung.

## Auslieferung und Prüfung

Windows: `virgin-0.4.0.exe`. Android: `virgin-arm64-v8a`, `virgin-armeabi-v7a`, `virgin-x86_64`. Keine Netzdateien und keine zusätzlichen Laufzeitdaten erforderlich. Die Android-Dateien sind eigenständige UCI-Programme, keine APK; sie wurden gebaut und als ELF geprüft, aber nicht auf einem Gerät ausgeführt.

Windows-Selbsttest und UCI-Prozesstest werden an der ausgelieferten Datei ausgeführt; Ergebnisse stehen in `selftest.txt` und `uci-smoke.txt`. Die Tests enthalten beide vollständigen Nutzerpartien, Perft, Farbspiegelung, Sonderzüge, Suche, Mehrkern-TT, taktische Zugerzeugung und die neuen Fesselungs-/Wiederholungsfälle. `manifest.json` enthält Größen und SHA-256-Prüfsummen.

Für einen Test der maximalen Engine-Stärke:

```text
setoption name UCI_LimitStrength value false
setoption name Style value Angriffslustig
setoption name Threads value 4
setoption name Hash value 32
```

Die Threadzahl passend zum Gerät wählen und für Versionsvergleiche gleich lassen. Ein Bot-Profil `v2200-...` kann die Engine absichtlich begrenzen und bedeutet nicht, dass die gemessene Spielstärke 2200 Elo beträgt. Die Einbindung in die Schach-Apps und die Vergleichspartien bleiben beim Nutzer.

Alte Auslieferungen, insbesondere `dist/meine-engine`, bleiben unangetastet. Der Quellcode und die bisherige Windows-Datei wurden vor dem Umbau gesichert.
