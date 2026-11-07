# Virgin 0.2.0 – Stand 31.10.2026

Diese Version ist ein Kandidat für Spielstärketests. Ein Elo-Gewinn ist nicht gemessen. Es wurden keine Partien gegen andere Engines gestartet.

## Änderungen

- Verlustreiche Schlagzüge stehen jetzt hinter den ruhigen Zügen. Vorher blieb ihr Sortierwert trotz SEE-Abzug höher als der sämtlicher Killer- und History-Züge.
- History-Werte sind begrenzt und werden bei Beta-Cutoffs aktualisiert. Erfolglose zuvor untersuchte ruhige Züge erhalten einen Malus. Die bisherige Addition in int16 konnte die Werte über den positiven Bereich hinauslaufen lassen.
- In der Ruhesuche werden nur taktische Züge sortiert; bei Schach bleiben alle Ausweichzüge erhalten. Berechnete Figurenangriffe werden innerhalb einer Bewertung für Mobilität und Königssicherheit wiederverwendet.
- Die Ruhesuche verwendet die vollständige Bewertung. Die bisherige Material-/PST-Näherung mit einem festen Abstand von 400 cp war insbesondere bei starken Königsangriffen keine sichere Schranke.
- Patt und Wiederholung werden in der Ruhesuche erkannt. Matt geht dem 50-Züge-Zähler vor.
- Die Hauptsuche besitzt jetzt ebenfalls eine Grenze für den Suchstack. Schachverlängerungen erhalten ein Ply-Budget; auch gesunde schachgebende Schlagzüge können verlängert werden. Ruhige Schachzüge werden vor dem Pruning auf jeder Tiefe erkannt.
- Aufeinanderfolgende Nullzüge werden verhindert; Wiederholungssuchen gehen nicht über einen künstlichen Nullzug hinweg. Die aktuelle Stellung wird am Suchstart nicht doppelt an die Schlüsselhistorie angehängt.
- Die gemeinsame Transpositionstabelle wird mit verteilten Sperren geschützt. Leser verwenden einen konsistenten lokalen Schnappschuss statt eines weiterhin veränderlichen Zeigers. Einträge bleiben 16 Byte groß. Die Sperren kosten etwas RAM und Laufzeit.
- UCI-Konfigurationsänderungen stoppen und verbinden zunächst die Suchthreads. Eine Startbestätigung verhindert, dass ein unmittelbar auf `go` folgendes `stop`, `quit` oder `setoption` vom Zurücksetzen des Stoppsignals verschluckt wird.
- Fesselungen und Abzugspotenzial werden nur bei passender Läufer-/Turmrichtung und genau einem Zwischenstein bewertet. Zuvor genügte eine gemeinsame geometrische Linie, sogar in falscher Zugrichtung oder hinter mehreren Blockern.
- `BetweenBB` enthält nur echte Zwischenfelder. Gleichfarbige Läufer ohne weiteres Material werden als tot erkannt; Läufer gegen Springer wird nicht mehr pauschal auf null gesetzt. Die Remisskalierung ungleichfarbiger Läufer greift jetzt bei Bauernendspielen mit Läufern.

Die 91 Gewichte, die Stilprofile und die Stärke-Schnittstellen bleiben verfügbar. Bewertungsgewichte wurden nicht neu getunt. Die UCI-Standardauswahl heißt im bestehenden Projekt **Ausgeglichen**. Für den gewünschten Angriffsstil bitte explizit **Angriffslustig** einstellen.

## Benutzung

Windows: `virgin-0.2.0.exe` als UCI-Engine in der Schachoberfläche auswählen.

Android: `virgin-arm64-v8a` ist die übliche Version für aktuelle ARM-Handys. Zusätzlich liegen `virgin-armeabi-v7a` für 32-bit ARM und `virgin-x86_64` für passende Emulatoren/Geräte bei. Es sind eigenständige UCI-Programme, keine APK. Sie benötigen nur Android-Systembibliotheken, keine Netz- oder Gewichtedateien. Der Build zielt auf API 21; die tatsächliche Einbindung und ausführbare Ablage richten sich nach der jeweiligen Schach-App und Android-Version.

Für volle Stärke mit Angriffsstil, in dieser Reihenfolge:

```text
uci
setoption name UCI_LimitStrength value false
setoption name Style value Angriffslustig
setoption name Threads value 1
setoption name Hash value 32
isready
ucinewgame
position startpos
go movetime 1000
```

Für kontrollierte Versionsvergleiche feste, identische Threadzahl und Hashgröße verwenden. `UCI_Elo 2000` oder ein begrenzendes Bot-Profil wären ein absichtliches Handicap. Die Elo-Namen im Katalog sind keine Messung der realen Engine-Stärke. Für einen Mehrkerntest kann `Threads` separat auf 2 oder 4 gesetzt werden.

## Prüfung und Grenzen

Der erweiterte Windows-Selbsttest umfasst Perft über sechs Referenzstellungen, Figuren-/Zobrist-Konsistenz, Stil- und Gewichteschnittstellen sowie neue Regressionen für Fesselung, tote Stellungen, Patt, Matt beim 50-Züge-Zähler, Umwandlung, History-Grenzen, Suchstack und die Zugreihenfolge. Ein Test mit vier kollidierenden Schreibern prüft die Konsistenz von TT-Schnappschüssen. Das ist kein vollständiger Ersatz für einen Thread-Sanitizer.

Der UCI-Prozesstest prüft Startmeldung, Suche mit einem und vier Threads, Konfigurationswechsel unmittelbar nach Suchstart, Zeitbegrenzung und Beenden. Ergebnisse und Größen stehen in den mitgelieferten Protokollen bzw. `manifest.json`.

Wichtig für deine Partietests: Die Korrekturen haben einen Laufzeitpreis. Ein kurzer Windows-Funktionsbenchmark mit einem Thread, Tiefe 7 über vier Stellungen, lieferte vor dem abschließenden Start/Stopp-Fix etwa **0,59 Mio. Knoten/s** für diesen Kandidaten gegenüber **0,87 Mio. Knoten/s** für den gesicherten Altstand. Die Suchbäume unterscheiden sich; daraus folgt weder ein Elo-Gewinn noch ein Elo-Verlust. Insbesondere die vollständige Blattbewertung und Pattprüfung benötigen zusätzliche Arbeit. Die Werte sind Einzelmessungen, kein belastbarer Geschwindigkeitsvergleich auf Android.

Android wurde für alle drei ABIs kompiliert; ELF-Format und Systembibliotheksabhängigkeiten wurden geprüft. Ohne verbundenes Android-Gerät ist kein Laufzeittest oder Import in eine Android-Schach-App erfolgt.

## Nächste sinnvolle Schritte nach den Partietests

1. Die Fehlerkorrekturen gegen die gesicherte Vorgängerversion messen; Zeitkontrolle und Threads gleich halten. Bei gemischtem Ergebnis die Änderungen einzeln prüfen, besonders vollständige Blattbewertung und Schachverlängerungen.
2. Ruhesuche und Bewertung profilieren: direkter Generator für Schlagzüge/Umwandlungen, schnelle Pattprüfung, inkrementelle Material-/PST-Summen, optional kleine Bauern-Hashtabelle. Diese Maßnahmen benötigen keine Netzdateien.
3. LMR mit Stellungstrend und History verknüpfen; anschließend kleine Continuation-History und verifizierte Nullzug-Suche in geeigneten Endspielen einzeln erproben. RAM-Kosten für ESP32 begrenzen.
4. Den vorhandenen Texel-Tuner vor neuen Gewichten überarbeiten: Trainings- und Validierungssplit nach ganzen Partien statt einzelnen Stellungen; `nicht im Schach` genügt nicht als Ruhefilter. Offene Partien am 220-Ply-Limit werden derzeit als Remis etikettiert. Nur acht deterministische Eröffnungen bieten wenig Vielfalt. Bessere Trainingsdaten können ausschließlich im Entwicklungsprozess bleiben; ausgeliefert werden nur kompakte Zahlenkonstanten.
5. Königssicherheit: tatsächliche sichere Schachmöglichkeiten und Zahl der beteiligten Angreifer berücksichtigen. Der aktuelle `SafeCheck`-Term zählt Angriffe auf die Königszone, keine nachgewiesenen sicheren Schachzüge. Freibauern zusätzlich nach Blockade, sicherem Vorstoß und Königsentfernung zum Umwandlungsweg bewerten.
6. Für ESP32 ein eigenes Speicherprofil vorsehen: Die aktuellen Turm-/Läufer-Angriffstabellen belegen allein **861.184 Byte** RAM, die beiden 64×64-Linientabellen weitere **65.536 Byte**; der Standard-Hash benötigt zusätzlich 32 MiB. Die kleine Dateigröße sagt deshalb wenig über ESP32-Eignung aus. Kleiner Hash, kompaktere Angriffserzeugung und ein Build ohne Match-/Tuning-Werkzeuge sind dafür spätere Schritte. Ein ESP32-Build ist hier nicht enthalten.

Das vorhandene Matchwerkzeug entscheidet bei Zuglimit nach Material. Das kann gerade Opfer- und Angriffsstile verzerrt bewerten. Für die Beurteilung der Spielstärke möglichst eine externe Partieauswertung mit passenden Abbruchregeln verwenden.

## Quellen zur Einordnung der Verfahren

- [Rofchade: Suchverfahren, gestufte Zugerzeugung, History und klassische Bewertung](https://rofchade.nl/?page_id=116)
- [David-Tabibi / Netanyahu: Verified Null-Move Pruning](https://arxiv.org/abs/0808.1125)
- [Texel-Tuner: Training klassischer Bewertungsgewichte](https://github.com/GediminasMasaitis/texel-tuner)
- [C++-Standardentwurf: Datenrennen und Nebenläufigkeit](https://eel.is/c++draft/intro.races)

Es wurde kein Fremdengine-Code übernommen.
