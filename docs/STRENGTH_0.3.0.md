# Virgin 0.3.0 – Carballo-Partie, 31.10.2026

Ziel bleibt, Carballo bei 1 Sekunde pro Zug klar zu schlagen. Dieses Ziel ist **noch nicht nachgewiesen**. Es wurden keine Matches gegen Carballo oder andere Engines gestartet. Dies ist ein neuer Kandidat für die Partietests des Nutzers, auf Basis von 0.2.0.

## Was die gelieferte Partie zeigt

Die vollständigen 65 Halbzüge wurden mit den Engine-Regeln nachgespielt. Ein neuer Selbsttest kontrolliert Legalität und Brett-/Zobrist-Konsistenz nach jedem Zug. Die ursprüngliche Diagnose ließ sich teilweise reproduzieren: 0.2.0 wählt mit einem Thread und ausgeglichenem Stil bei 1 Sekunde unter anderem De3, h4 und g3.

Die konkrete Diagnose zum Bauernsturm wurde nach dem Replay eingegrenzt: Weiß greift den schwarzen König auf c8 an. **h4 selbst bekommt hier keinen Bauernsturm-Bonus gegen c8.** Die wichtigeren Probleme sind der zu günstig bewertete Bauernschild, das fehlende vorgelagerte Einbruchsfeld h3, unpräzise Angriffseinheiten und ein angreifender Stil, der zusätzlich die defensive Grundlage heruntersetzt. g3 erhält außerdem statische Vorteile dafür, den Springer f4 mit einem Bauern anzugreifen und dessen Vorpostenbonus zu beseitigen; solche nominellen Vorteile müssen gegen die konkrete Königsgefahr aufgewogen werden.

Die Materialbeschreibung braucht eine Korrektur: Nach 26.Sd5 Txd5 **27.exd5 gewinnt Weiß die Qualität zurück**. Der im Partieverlauf große negative Wert ist damit nicht allein als zusätzlicher Materialverlust zu erklären. Nach dem nachfolgenden 27...hxg4 ist das Material wieder ausgeglichen, der weiße König bleibt aber stark gefährdet. Die schwarze Wiederholung am Ende ist kein Beleg für eine ausgeglichene Stellung.

## Änderungen

1. **Königszone:** Neben den direkten Nachbarfeldern werden die drei Felder zwei Reihen vor dem König einbezogen. Dadurch zählt beispielsweise h3 bei einem weißen König auf g1. Die Erweiterung ist nach Verteidigerfarbe gespiegelt.
2. **Bauernschild:** Ein Bauer zwei Reihen vor dem König zählt nur noch teilweise als Schutz. g2 und g3 sind für einen König auf g1 nicht mehr gleichwertig. Angegriffene, nicht von eigenen Bauern gedeckte Felder in der erweiterten Zone werden zusätzlich bestraft; Doppelangriffe verschärfen die Strafe, wenn der Gegner eine Dame besitzt.
3. **Schachpotenzial:** `KingSafety.SafeCheck` zählt statisch mögliche, hinreichend geschützte Schachfelder statt pauschal aller Ringangriffe eines nicht von Bauern bedrohten Angreifers. Die Abschätzung ist kein vollständiger Legalitätsbeweis; Fesselungen und veränderte Linien nach einem Zug prüft weiterhin die Suche.
4. **Angriffslustig:** Material bleibt bei 100 %, Bauernstruktur bei 100 %, Königssicherheit bei 110 %. Offensivboni bleiben höher als neutral, sind aber maßvoller. Opfertoleranz und Damentauschvermeidung sind reduziert. Die Verstärkung der Angriffsbewertung bei Materialrückstand ist generell begrenzt; sie steigt nicht beliebig mit weiterem Materialverlust.
5. **Bauernsturm:** Blockierte Vorstöße und das Öffnen des eigenen Königsflügels erhalten weniger Bonus. Vorstöße gegen einen König im Zentrum werden nicht pauschal wie ein Flügelsturm belohnt.
6. **Suche:** LMR reduziert innerhalb der Hauptvariante und bei guten History-Werten weniger. Es wurden keine Züge dieser Partie fest einprogrammiert und keine Sollzüge auf die konkrete Partie trainiert.
7. **Tempo:** Die Ruhesuche erzeugt direkt Schlagzüge und Umwandlungen. Bei Schach bleiben alle Ausweichzüge erhalten. Für die Pattprüfung genügt häufig ein vorab geprüfter legaler Bauernvorstoß; andernfalls greift die vollständige Zugerzeugung. Ein Vergleichstest prüft die direkte taktische Zugliste gegen die gefilterte vollständige Liste über viele legale Stellungen, einschließlich Sonderzügen.
8. **Diagnosefehler:** Die Konsistenzprüfung verwendete für den En-passant-Schlagbauern die falsche Farbe/Richtung. Der Fehler wurde durch 24...h5 in dieser Partie sichtbar und ist korrigiert. Die En-passant-Zugerzeugung selbst war davon nicht betroffen.

Alle bisherigen 91 Gewichtsnamen und die fünf Stilprofile bleiben verfügbar. Keine Netzdateien, keine neue Laufzeit-Datendatei, keine externen Bibliotheken. Alte gespeicherte Einzelgewichte können die neue Abstimmung überschreiben: für den Vergleich das kompilierte Profil frisch auswählen und keine alte Gewichtedatei nachladen.

## Stellungstests mit 1 Sekunde

Die JSON-Protokolle enthalten die genaue FEN, statische Bewertung, Zeit, Tiefe, Knoten und Zug. Vor jedem Fall werden neue Partie und Hash-Löschung angefordert. Getestet wurden volle Stärke und jeweils die Stile Ausgeglichen und Angriffslustig. Einthread- und Vierthread-Läufe wurden nacheinander durchgeführt. Mehrkernsuche und Zeitbegrenzung sind nicht deterministisch.

Im abschließenden Vierthread-Lauf spielte 0.3.0 in **beiden** Stilen:

| Stellung | Wahl bei 1 s / 4 Threads |
|---|---|
| vor 13.De3 | 13.d3 |
| vor 15.h4 | 15.Sxe5 |
| vor 16.g3 | 16.Tfe1 |

Die Schwäche ist damit **nicht vollständig beseitigt**: Im Einthread-Lauf spielt der neue Angriffsstil vor Zug 15 weiterhin h4 und vor Zug 16 weiterhin g3. Im ausgeglichenen Einthread-Stil werden diese beiden Züge in diesem Lauf vermieden. Eine günstig aussehende Auswahl in wenigen Stellungen ist kein Elo-Nachweis. Die Eröffnungsbewertung vor Zug 9 bleibt positiv (im Angriffsstil +0,78 statt zuvor +1,09 in den jeweiligen Einthread-Läufen); die Eröffnungsüberschätzung ist nicht vollständig gelöst und wurde nicht durch kosmetisches Herunterskalieren der Anzeige versteckt.

Im kurzen Funktionsbenchmark mit einem Thread und Tiefe 7 wurden zuletzt etwa 0,87 Mio. Knoten/s für 0.3.0 und 0,77 Mio. für 0.2.0 gemessen. Vorherige Einzelmessungen schwankten stärker. Suchbäume und Bewertungen unterscheiden sich; der Wert ist ein Anhaltspunkt zum Laufzeitverhalten und kein Spielstärkevergleich.

## Benutzung

Windows: `virgin-0.3.0.exe` als UCI-Engine auswählen. Android: `virgin-arm64-v8a` für übliche aktuelle ARM-Handys, `virgin-armeabi-v7a` für 32-bit ARM, `virgin-x86_64` für passende Emulatoren/Geräte. Es sind UCI-Programme, keine APKs. Die Android-Builds wurden auf ELF-Format und Bibliotheksabhängigkeiten geprüft, aber nicht auf einem Android-Gerät ausgeführt.

Für den Carballo-Vergleich bitte kein Elo-Handicap verwenden:

```text
setoption name UCI_LimitStrength value false
setoption name Style value Angriffslustig
setoption name Threads value 4
setoption name Hash value 32
```

Die Threadzahl muss zur Testbedingung passen; für einen fairen Vergleich beider Virgin-Versionen identisch setzen. Ein `Bot` wie `v2200-...` kann die Engine absichtlich begrenzen. Die Zahl 2200 ist dabei keine Messung ihrer tatsächlichen Stärke. Den Stil Ausgeglichen ebenfalls separat zu prüfen hilft, Suchverbesserung und Stilrisiko auseinanderzuhalten.

Die alten Auslieferungsordner und `dist/meine-engine` bleiben unverändert. `manifest.json` enthält Größen und SHA-256-Prüfsummen. `selftest.txt` und `uci-smoke.txt` dokumentieren die Windows-Prüfung; die vier `case-*.json`-Dateien die Stellungstests vor und nach dem Umbau. Die Tests gegen andere Engines bleiben beim Nutzer.

Als Einordnung des verwendeten Grundverfahrens: [Rofchade beschreibt gestufte Schlagzugerzeugung, History und klassische Suche](https://rofchade.nl/?page_id=116). Die Änderungen hier sind eigene Implementierungen im vorhandenen Virgin-Code.
