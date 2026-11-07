# Virgin 0.7.0 – Adaptive aus CynusTactics

31.10.2026. Der Nutzer wollte die geprüfte adaptive Patricia-Zugauswahl aus eChessTactics als eigene, vorrangige UCI-Option. Das Projekt liegt lokal unter `Documents/CynusTactics`. Übernommen wurden die Entscheidungsregeln aus `TalChooser.kt` und `tal/tal_chooser.py` sowie deren feste Werte aus `app/src/main/assets/tal/tal_params.json`. Das Quellprojekt wurde nur gelesen. Referenzkopien und SHA256 stehen unter `reference/`.

## Einschalten

In der Schachoberfläche die neue Checkbox **Adaptive** aktivieren, oder per UCI:

```text
setoption name Adaptive value true
ucinewgame
position startpos
go movetime 1000
```

`Adaptive` ist standardmäßig aus. Aktiviert übersteuert es sämtliche anderen UCI-Optionen: festes Angriffslustig-Profil mit voller Bewertungskenntnis, keine Elo-Handicaps, keine künstlichen Knoten-/Tiefenkappen, kein Bewertungs- oder Zugrauschen und keine zufälligen Patzer. Hash ist fest 32 MiB; Threads automatisch bis vier. Andere `setoption`-Befehle werden ignoriert, auch während einer Suche; sie unterbrechen diese nicht. Beim Ausschalten werden die vor Aktivierung gültigen Gewichte, Kenntnisfaktoren, Stärke-, Hash- und Thread-Einstellungen wiederhergestellt. Währenddessen ignorierte Einstellungen werden nicht nachträglich angewandt.

UCI-Suchaufträge wie `go movetime`, Uhrzeiten, `depth`, `nodes`, `stop` und `quit` bleiben verbindlich. Volle Rechengeschwindigkeit bedeutet keine künstliche Schwächung der Suche; die bewusste Anpassung erfolgt erst bei der Zugwahl. Ohne Zeit-/Knoten-/Tiefenvorgabe rechnet ein einfaches `go` wie die Originalanwendung 2500 ms. `go infinite` bleibt unbegrenzt bis `stop`.

## Übernommenes Verhalten

Virgin berechnet bis zu fünf beste Zugkandidaten mit eigenen Bewertungen und tatsächlich gesuchten Hauptvarianten. Eine begonnene, aber durch die Zeitgrenze unvollständige Kandidatenrunde wird verworfen; die letzte vollständige Runde liefert die Auswahl. Bei weniger legalen Zügen werden entsprechend weniger Kandidaten benutzt. Reicht selbst die Zeit für die erste Runde nicht, erfolgt eine legale Standardantwort ohne erfundene Adaptionsmessung.

Die Auswahlregeln und ihre Reihenfolge entsprechen der geprüften Funktion:

- Erste acht Halbzüge: bester Zug.
- Spieler schlechter oder gleich −300 cp oder nahes Matt: bester Zug.
- Laufende Anpassungs-Schonfrist oder geschützte Taktik: bester Zug. Materialgewinn ab 250 cp über höchstens sechs Halbzüge schützt eine Kombination für weitere vier Halbzüge. Die letzte Antwort des Gegners wird berücksichtigt.
- Spieler erreicht +200 cp: Halten wird für diese Partie gespeichert. Ziel +200, zulässiger Abstand zum besten Zug höchstens 250 cp; Sicherheitsprüfung bleibt aktiv.
- In stabil ausgeglichener Stellung kann die Engine bei passender Kandidatenleiter etwa 120 cp nachgeben (80–130 erlaubt). Frühestens ab Halbzug 14, drei stabile eigene Züge, mindestens vier Kandidaten, Pausen von zwölf Halbzügen, höchstens drei Anpassungen; danach acht Halbzüge Schonfrist.
- Genauigkeit: letzte zwölf gemessene Spielerzüge, bis 30 cp Verlust gilt als gut; vor sechs Messungen keine solche Anpassung. Ab 90 % Genauigkeit kein bewusstes Nachgeben über die Leiter.
- Spieler zwischen −150 und −300 cp: falls ein plausibler Kandidat existiert, begrenzte Hilfe bis 80 cp je Schritt, maximal fünf Kandidaten und kein Materialverlust über 100 cp in den ersten vier Halbzügen. Andernfalls bester Zug.
- Sonst Ausgleich Richtung 0, höchstens 80 cp schlechter als bester Zug. Unter beinahe gleichwertigen Kandidaten dieselbe Zufallsregel (15 %).

Die Materialprüfung verwendet wie das Original Bauer 100, Springer/Läufer 300, Turm 500, Dame 900 und bewertet Nettoverluste nach Rückschlägen. Sie berücksichtigt legal nachgespielte Hauptvarianten einschließlich Sonderzügen. Der beste Kandidat bleibt immer erlaubt. Die Regeln sind eingebettet; die Referenz-JSON wird zur Laufzeit nicht geladen.

`ucinewgame` setzt den gesamten adaptiven Zustand zurück. Wiederholte Suche derselben Stellung zählt nicht als weiterer Spielerzug. Die Zugnummer wird aus dem FEN-Zugzähler ermittelt. Bei Rücksprung, Seitenwechsel oder einem abweichend gespielten eigenen Zug wird der veraltete Verlauf verworfen; bei einem nackten FEN ohne Zugliste sind nur die darin enthaltenen Verlaufsdaten bekannt. Die Anpassung ist für die Engine auf einer festen Seite gegen den Spieler gedacht. Für vollständige Wiederholungserkennung weiterhin `position startpos moves ...` verwenden.

Diagnosebeispiel:

```text
info string adaptive helfen player -220 after -150 accuracy 67 best ... selected ... samples 6 helped 1 adjusts 0
```

Alle Angaben hinter `player` und `after` sind aus Sicht des Gegenspielers, wie im Original. Normale UCI-Bewertungen bleiben aus Sicht der Engine. Virgin liefert eigene Kandidaten; gleiche Regeln bedeuten keine identischen Züge oder Bewertungen wie Patricia. Ebenso sind durch TT-Abkürzungen eventuell kürzere Virgin-Hauptvarianten eine Grenze der Materialvorschau. Zufallsauswahl hat dieselbe Verteilung, keinen plattformübergreifend identischen Zufallsstrom.

## Prüfung

- 111 interne Windows-Prüfungen bestanden, einschließlich Adaptivregeln und fünf gleich tiefen Kandidaten gegen separate Vollfenstersuchen bei ausgeschalteten selektiven Kürzungen.
- Direkter Vergleich des C++-Entscheiders mit der ursprünglichen Python-Funktion: **680 Fälle, 0 Unterschiede**, einschließlich aller zehn Entscheidungsmodi, gezielter Zustände, Zufallsauswahl und Materialvarianten aus beiden Verlustpartien. Verglichen wurden Zug, Modus, Bewertungen, Genauigkeit und Folgezustand.
- UCI-Prozesstest: Option vorhanden, alle Fremdoptionen wirkungslos, volle Suche trotz zuvor eingestellter Elo 700, fünf legale Hauptvarianten gleicher Tiefe, kein doppeltes Genauigkeitsmessen, Rücksetzen bei neuer Partie, responsive Stop-Funktion, sichere Endstellungen und einzelne legale Antwort. Ein-Sekunden-Proben 1000 bzw. 969 ms im dokumentierten Windows-Lauf. Ausschalten stellt auch den vorherigen Elo-Handicap wieder her.
- Normalmodus-Benchmark gegenüber 0.6.0 identisch: 101101 Knoten bei Tiefe sechs/einem Thread über vier feste Stellungen. Das ist ein Regressionstest, kein Spielstärkenachweis.
- Android ARM64, ARM32 und x86_64 mit NDK 27/API 21 gebaut, ELF-Architektur und PIE geprüft. Nur libc/libm/libdl benötigt. Kein angeschlossenes Gerät: keine Android-Ausführung oder Installation geprüft.

Es wurden keine Gegnerpartien gespielt. Dieser Modus soll dem Spieler kontrolliert entgegenkommen und ist kein zusätzlicher maximaler Spielstärkemodus. Für reine Carballo-Stärketests `Adaptive=false` und die gewohnten Einstellungen verwenden.

## Dateien

- `virgin-0.7.0.exe`: 478208 Bytes (467.0 KiB).
- `virgin-arm64-v8a`: 866784 Bytes (846.5 KiB).
- `virgin-armeabi-v7a`: 588324 Bytes (574.5 KiB).
- `virgin-x86_64`: 904896 Bytes (883.7 KiB).

Windows als UCI-Engine hinzufügen. Für normale aktuelle Android-Geräte `virgin-arm64-v8a` importieren; für ältere 32-Bit-Geräte `virgin-armeabi-v7a`, für x86-Geräte/Emulatoren `virgin-x86_64`. Dies sind Engine-Binärdateien, keine APKs. Keine Netzdateien und keine weiteren Laufzeitdaten erforderlich. Die App-Auswahl und den Import übernimmt der Nutzer selbst.

Alle älteren Veröffentlichungen bleiben erhalten; 0.5.0 und 0.6.0 wurden gegen ihre Manifeste geprüft. `dist/meine-engine` nicht verändert. Sicherung vor dieser Änderung: `backups/before-0.7-20261031-144237`. Alle Binärprüfsummen in `manifest.json`. Testwerkzeuge unter `tools/virgin_adaptive_*.py`; der separate CLI-Modus `--adaptive-probe` dient ausschließlich dem Vergleich des Entscheiders mit dem Original.
