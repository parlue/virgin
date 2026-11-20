# Globales Eröffnungsbuch und adaptive Zugvielfalt

`OwnBook` ist eine globale UCI-Checkoption mit Standard `true`:

```text
setoption name OwnBook value false
setoption name OwnBook value true
```

Das Buch gilt für volle Stärke, sämtliche Elo-Stufen, alle Stil-Bots und den
adaptiven Modus. Die Option lässt sich auch bei aktivem adaptivem Modus ändern;
die Änderung bleibt beim Verlassen dieses Modus erhalten.

## Umfang und Speicher

90 kuratierte Varianten mit jeweils zehn Halbzügen (fünf volle Züge). Enthalten
sind offene Spiele, Sizilianisch, Französisch, Caro-Kann, Skandinavisch, Pirc,
Moderne Verteidigung, Aljechin, Damengambit, Slawisch, London, indische
Verteidigungen, Holländisch, Englisch, Réti, Larsen und Bird. Dies ist eine
Auswahl spielbarer Fortsetzungen, keine statistische Top-30-Liste.

Die Zugfolgen belegen einschließlich Trennmarkierungen **1.980 Byte**. Mit der
Startstellungszeichenfolge sind es **2.037 Byte**. Es gibt keine zusätzlichen
Buchgewichte, Zeiger- oder Indextabellen im Programm. `OpeningBook::dataBytes`
zählt diese Daten; ein `static_assert` verhindert einen Build ab 10.000 Byte.
Das Budget bezieht sich auf die Buchdaten, nicht die gesamte Engine oder die
neue Programmlogik und Selbsttests.

Die Tabellen liegen konstant im Programmspeicher. Sie werden nicht vollständig
in einen Heap-Index kopiert. Zur Abfrage entstehen lokale Brettkopien und ein
kleiner Vektor der unterschiedlichen legalen Folgezüge.

## Zugauswahl

An einer bekannten Stellung wird unter den unterschiedlichen Buchzügen
**gleichverteilt** gewählt. Mehr Varianten hinter einem Zug machen ihn nicht
wahrscheinlicher. Die Auswahl ist unabhängig von Bewertung, Elo-Handicap und
Stil. Zufall kann Wiederholungen zwischen Partien ergeben. Eine erneute Abfrage
derselben Stellung behält die Wahl; `ucinewgame` setzt sie zurück.

Buchzüge werden ohne Suchbewertung des Buchzugs ausgegeben: `info string book selected ... data_bytes
2037`, dann `bestmove`. Es wird keine unberechnete Bewertung ausgegeben. Der
adaptive Modus sammelt während der Buchphase keine erfundenen Genauigkeitswerte.
Die getrennte Live-Performance-Messung bereits bestätigter Spielerzüge bleibt
auch während der Buchphase aktiv, sofern sie eingeschaltet ist.

Nach Halbzug 10 oder bei unbekannter Stellung rechnet der eingestellte Modus.
Transpositionen werden über den Stellungsschlüssel inklusive Zugrecht,
Rochaderechten und en passant erkannt; Halbzugzähler sind kein Buchkriterium.
Endliche `go`-Aufträge können Buchzüge liefern. `go infinite`, `go ponder`,
explizite `searchmoves` und die interne Leistungsvorbereitung verwenden die Suche.
Zum Analysieren mit fester Tiefe ebenfalls `OwnBook=false` setzen.

## Adaptiver Modus ohne Buch / nach Buchende

Der adaptive Chooser wählt immer zufällig zwischen sicheren Kandidaten innerhalb
von **20 Centipawns** des vom jeweiligen adaptiven Ziel gewählten Kandidaten.
Es gibt keine 15-Prozent-Hürde und keine zusätzliche Sperre anhand der vorherigen
Suchtiefe. Der erlaubte Bewertungsverlust des adaptiven Ziels wird nicht erweitert.

Taktische Materialgewinne und laufende Taktikfolgen werden vor den Eröffnungs-
und Anpassungsregeln geprüft und behalten den stärksten Zug. Bei Mattbewertungen
oder wenn der König im Schach steht wird nicht randomisiert. Alternativen müssen
eine legale, ausreichend lange berechnete Hauptvariante vorweisen und dürfen im
geprüften Horizont gegenüber der gewählten Variante keinen zusätzlichen
Materialverlust verursachen. Dieser Schutz beruht auf der verfügbaren Suche und
ist keine Garantie gegen Taktiken außerhalb ihres Horizonts.

## Pflege und Prüfung

Lesbare Quelle: `data/opening_book.txt` (SAN). Namen und Zugkonventionen orientieren
sich am CC0-Katalog https://github.com/lichess-org/chess-openings.

```text
python tools/generate_opening_book.py
build086.cmd
build/086-win/virgin.exe --selftest
python tools/test_opening_book.py build/086-win/virgin.exe
python tools/test_086_uci.py build/086-win/virgin.exe
```

Der Generator benötigt `python-chess`; die Engine hat keine neue Abhängigkeit
und benötigt die Textdatei zur Laufzeit nicht. Die Tests prüfen die Legalität
aller Buchfolgen, die Datengrenze, sämtliche 215 Bot-Profile mit beiden Farben,
Schaltbarkeit, Buchende, Analyse, `searchmoves`, Zufall ohne Buch sowie
Taktik- und Mattpriorität.
