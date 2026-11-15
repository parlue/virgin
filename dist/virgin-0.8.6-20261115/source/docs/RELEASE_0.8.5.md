# Virgin 0.8.5 Beta

Dirk D. Sommerfeld, 7. November 2026. Grundlage: Virgin 0.8.4.

## Neue Partieleistungsanzeige

- UCI-Option `LivePerformance`, standardmaessig aus. Adaptive aktiviert die Berechnung immer, unabhaengig vom optionalen Normalmodus-Schalter.
- Abfrage `performance`. Ausgabe als `info string performance white ... black ... window 3 ...`. Nur eine ausdrueckliche Abfrage gibt die Anzeige aus; es besteht keine Anzeigepflicht fuer GUIs.
- Beide Farben starten bei `>2800`. Mittelwert der letzten drei eigenen Zugabzuege, Ausgabe in Hunderterschritten. Das ist eine entworfene Momentaufnahme des Partieverlaufs, keine kalibrierte Turnier-Elo oder Einstufung des Spielers.
- Kleine Ungenauigkeiten ohne Damen und mit wenigen Figuren wirken milder. Grosse Einbrueche bleiben voll wirksam.
- In Verluststellung keine automatische Erholung durch drei gute, aber stellungserhaltende Zuege. Ein echter Positionsgewinn kann begrenzte Erholung ermoeglichen. Genutzte Adaptive-Chancen koennen helfen; verpasste Chancen erzeugen kein Guthaben. Positionspendeln erzeugt kein mehrfaches Guthaben.
- Werte stammen hier aus Virgins eigener voller Suchstaerke, nicht aus einem externen Stockfish. Die Zahlen koennen daher vom Offline-Prototyp abweichen. Die Handicaps der eigentlichen Spielsuche werden nach einer Messung vollstaendig wiederhergestellt. Der gewaehlte Spielpfad und die MultiPV-Einstellung der GUI bleiben erhalten.
- Kandidatenvergleiche verwenden einen vollstaendigen gemeinsamen Such-Tiefenstand. Liegt der gespielte Zug nicht in den vorbereiteten Kandidaten, wird er zusammen mit den Referenzzuegen nachanalysiert. Es gibt keinen Vergleich eines tief untersuchten Favoriten mit einer bloss statisch bewerteten Alternative.

## GUI-Anbindung

Die kurze Abrufanleitung liegt in `APP_PERFORMANCE_0.8.5.md`.

Die GUI uebermittelt die Partie wie ueblich als `position startpos moves ...` oder als feste Ausgangs-FEN plus fortlaufende Zugliste. Eine Folge immer neuer FENs ohne Zugliste enthaelt keine Information, welcher Zug wirklich gespielt wurde; daraus werden keine erfundenen Zugbewertungen erzeugt.

Berechnung erfolgt im Hintergrund der naechsten normalen `go`-Anforderung; eine reine Abfrage loest keine Suche aus. Bereits analysierte echte Enginezuege werden aus dem Kandidatencache uebernommen. `pending` zeigt ausstehende Zuege. `white_samples`/`black_samples` zaehlen insgesamt ausgewertete eigene Zuege; das Rechenfenster bleibt trotzdem drei. `white_ply`/`black_ply` geben den zuletzt bewerteten Halbzug relativ zur uebermittelten Ausgangsstellung an. `revision` ermoeglicht Aenderungserkennung. Nach `ucinewgame`, Zuruecknehmen oder Aendern bereits ausgewerteter Zuege wird der passende Zustand zurueckgesetzt.

Spaete Aktivierung bzw. ein grosser Partienimport startet mit den letzten sechs Halbzuegen; `skipped` macht ausgelassene Historie sichtbar. Kein langer unsichtbarer Partienanalyse-Lauf vor dem naechsten Enginezug. Bei sehr kleinen Zeitbudgets werden Messungen verschoben und als ausstehend gemeldet. Grundbudget bis zu 100 ms je `go`, bei knapper Bedenkzeit reduziert und von der anschliessenden eigentlichen Suchzeit abgezogen. Cachetreffer sparen diese Zeit weitgehend.

Optional fuer eine angepasste GUI: Nach dem eigenen echten Enginezug `position ... moves ...` aktualisieren und `performance prepare` senden. Das bereitet fuer bis zu 250 ms volle Kandidaten der Spielerstellung vor; Abschluss `info string performance prepared depth N`. Ein normales `stop` kann dies unterbrechen. Dies ist eine ausdrueckliche Virgin-Erweiterung, kein automatisch gestartetes Standard-UCI-Pondern. Herkoemmliche GUIs brauchen sie nicht.

Ein mit `go ponder` nur angenommener Gegenzug wird nicht als tatsaechlich gespielt gewertet. Nach Ponderhit oder einer neuen bestaetigten Stellung wird die ausstehende Messung beim naechsten normalen Such-/Vorbereitungsdurchlauf abgearbeitet. Eine Abfrage bleibt auch waehrend der Suche eine schnelle Momentaufnahme.

## Bisherige Plattformen und zusaetzliche Linux-Ziele

Windows x64 UCI und DLL; Android OEX/APK und AAR fuer arm64-v8a, armeabi-v7a, x86_64, x86; Web/WASM; ESP32-S3 in drei bisherigen Profilen. Linux x86_64 sowie ARMv6/ARMv7 sind zusaetzliche Ziele. Tatsaechlich gebaute und gepruefte Dateien stehen im Paketmanifest. Android und Linux haben unterschiedliche Laufzeitumgebungen und werden separat ausgeliefert.

Keine GUI wurde installiert, kein Geraet geflasht und keine Website veroeffentlicht. Core2 ist kein Ziel dieses Updates. Keine neuen Bewertungsgewichte und kein Nachweis eines Spielstaerkegewinns; Schwerpunkt ist die optionale Partieleistungsanzeige.

## Parameter des ersten Entwurfs

Lichess-artige Gewinnchancen-/Genauigkeitskurve, interner Start 2900, 40 Leistungspunkte pro gemittelter Abzugseinheit, Anzeigeuntergrenze 400. Verlustanker ab -150 cp, Freigabe ab -50 cp. Innerhalb der Verlustzone Erholung erst ab mindestens 25 cp und einem Chancen-Prozentpunkt Verbesserung; Erholungsbudget 40 Punkte pro Chancen-Prozentpunkt. Diese Werte sind bewusste Testparameter, keine aus dem bekannten Rating des Testspielers abgeleiteten Konstanten.

Quellen der mathematischen Grundkurven: https://lichess.org/page/accuracy und https://github.com/lichess-org/lila/blob/master/modules/analyse/src/main/AccuracyPercent.scala . Unsere Anzeigeskala und Zustandsregeln sind davon getrennte eigene Entscheidungen.
