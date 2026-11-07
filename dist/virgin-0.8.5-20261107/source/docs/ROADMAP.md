# Roadmap

Meilensteine mit Status. Erledigte Punkte werden hier abgehakt, damit der
Fortschritt nachvollziehbar bleibt.

## M1 - Fundament (erledigt)

- [x] CMake-Projekt, ein Build für Windows und Android
- [x] Bitboards inkl. Magic Bitboards (zur Laufzeit gesucht, keine Dateien)
- [x] Board, FEN ein/aus, Zobrist-Hashing (inkrementell + Selbstprüfung)
- [x] Movegenerator (Rochade, En passant, Umwandlung), Legalitätsprüfung
- [x] Perft korrekt für 6 Standardstellungen
- [x] UCI-Grundgerüst inkl. `go` mit allen Zeit-/Tiefen-/Node-Parametern
- [x] Eingebauter Selbsttest `virgin --selftest` (reines C++)

## M2 - Suchkern (erledigt)

- [x] Iterative Vertiefung, Alpha-Beta, PVS-artige Nullfenster-Suche
- [x] MVV-LVA-Zugordnung, Wurzel-Sortierung
- [x] Zeitmanagement (`wtime/btime/winc/binc/movestogo/movetime`)
- [x] Suche läuft in eigenem Thread (`stop`/`ponderhit` funktionieren)
- [x] Transpositionstabelle (32 MB Standard, `Hash`-Option, `Clear Hash`)
- [x] Quiescence-Suche + SEE (keine losen Schlagzüge mehr)
- [x] Killer, Countermove, History-Heuristik

## M3 - Suchverfeinerung (erledigt)

- [x] Null-Move-Pruning (mit `hasNonPawnMaterial`-Prüfung)
- [x] Late Move Reductions (log-Formel) mit Re-Search
- [x] Late Move Pruning, Futility, Reverse Futility, Razoring
- [x] Internal Iterative Reduction, Schach-Erweiterung
- [x] Aspiration Windows, Mate-Distance-Pruning
- [x] Wiederholungs- und 50-Züge-Erkennung in der Suche
- [x] Zeitmanagement mit Soft-/Hard-Limit

Gemessen: 1,35 M Nodes bei Tiefe 11 (937k N/s), Tiefe 10-12 im Mittelspiel,
Matt-in-1 wird gefunden, hängende Dame wird geschlagen.

## M4 - Bots: Stil und Stärke anwenden (erledigt)

- [x] Stilprofile in C++ (`src/botdata.cpp`): 5 Stile mit allen Gewichten
- [x] `setoption name Style value <Name>` wendet die Stilgewichte an
- [x] `setoption name Bot value v1200-stier` setzt Stil + Elo in einem Schritt
- [x] Elo-Handicap auf die Suche: `MaxDepth`, `NodeLimit`, `MoveError`,
      `BlunderPct` (verifiziert: v700 spielt Tiefe 2, v2000 Tiefe 10)
- [x] Wissens-Skalierung je Bewertungsgruppe (700 Elo kennt Terme nur zu 25 %)
- [x] `setoption name Boldness value 10..50` mit Interpolation zwischen den Stilprofilen
- [x] `listbots` liefert den kompletten Katalog (215 Bots)
- [x] `--gen-data`, `--selftest`, `--selfplay` (alles in C++)
- [ ] Eröffnungsbuch mit menschlicher Zugwahl
- [ ] Zeitverhalten mit menschlichen Pausen (statt fester Bedenkzeit)

## M5 - Volle Evaluation, Tuning, Kalibrierung (teilweise erledigt)

- [x] Königssicherheit: Angriffseinheiten mit Sättigung, Bauernschild,
      offene/halboffene Linie am König, Angreifer im Königsring, sichere
      Angreifer, unrochiert-Strafe
- [x] Offene Linien: offen/halboffen, 7. Reihe, verbundene Türme, gegen den
      König, eingesperrter Turm
- [x] Bauernstruktur: Doppel-, Isolier-, Rückstands-, Verbund-, Frei- und
      Kandidatenbauern, Doppelbauern vor dem eigenen König
- [x] Mobilität je Figur (ohne Felder, die von feindlichen Bauern gedeckt sind)
- [x] Raum (sichere Mittelfelder), Bedrohungen (angegriffen von geringerwertiger
      Figur, hängend, gefesselt, aufgedeckter Angriff)
- [x] Figurendetails: Läuferpaar, schlechter Läufer, Außenposten, Fianchetto
- [x] Komplexität/Schärfe als eigener Term (Werkzeug für "ängstlich vs. Stier")
- [x] Aggression: König-Angriff, Bauernsturm, Initiative, Opfer-Toleranz,
      Damentausch-Vermeidung
- [x] Endspiel: Königsnähe zu Freibauern, König-Aktivität
- [x] Remis-Skalierung (ungleichfarbige Läufer, unzureichendes Material)
- [x] `Misc.Contempt` im Suchbaum verdrahtet (Remis wird mit -Contempt bewertet)
- [x] Lazy Evaluation (Material/PST vorgeschaltet)
- [x] PST-Basis aus veröffentlichten, getunten Tabellen (PeSTO, `src/pst_pesto.h`,
      erzeugt von `tools/extract_pesto.ps1`); die `Pst.*`-Gewichte sind nur noch
      Deltas darauf, Stile/Bots/Tuner funktionieren unverändert weiter
- [x] A/B-Werkzeug: `--match` gegen das eigene Vorgänger-Binary (gleiche
      Bedenkzeit, gepaarte Stichprobe) - für Bewertungsänderungen deutlich
      aussagekräftiger als der Spot-Lauf gegen Stockfish
- [x] 7 Eval-Einzeltests im Selbsttest (Material, Bauern, Freibauer,
      Königssicherheit, Mobilität, Remis-Skalierung)
- [x] Texel-Tuning aller Parameter (Engine-Modus `--tune`, in C++): Selfplay-Daten,
      K-Scan, Koordinatenabstieg, Train/Validation-Split und Freigabe nur bei
      besserem Validierungsfehler
- [ ] Größere Datensätze (10⁴-10⁶ Stellungen) und Tuning in Stufen
      (Material, dann PST, dann Rest) für belastbare Gewinne
- [ ] Getunte Basis in die Stilprofile übernehmen (Stile als Deltas statt
      absoluter Werte, damit die 215 Bots die Tuning-Gewinne erben)
- [ ] Stil-Messung aus Partien (Engine-Modus `--style-report`)
- [x] Wettkampf gegen eine fremde UCI-Engine (`--match`, in C++): Partien mit
      fester Bedenkzeit, PGN-Protokoll, Rochade-/En-passant-/Umwandlungsprüfung,
      Matt/Patt/50-Züge/Wiederholung/Material-Adjudikation
- [x] Spot-Suche im Wettkampf-Modus: Elo-Achse, danach Gegner-Bedenkzeit
      (1000 -> 30 ms), Abbruch nicht mehr gewinnbarer Stufen
- [ ] Elo-Kalibrierung gegen Referenzengines: mit `--match` gegen Stockfish
      (`UCI_LimitStrength`/`UCI_Elo`) gemessen; die eigene 700-2800-Skala noch
      nicht gegen die Messung geeicht (`--calibrate` mit Ordo wäre die Ausbaustufe)

**Wichtig:** Die Terme sind implementiert und einzeln verifiziert, aber die
Startgewichte sind geschätzt. Im Selfplay sieht man das an Zügen wie `1.g4`,
`1...f6` oder `a7a5` - die Terme sind zu stark gewichtet, bis Texel-Tuning sie
kalibriert. Die Suche selbst spielt solide (nach 1.e4 findet sie `d7d5`).

## M7 - Stärke messen und heben (in Arbeit)

- [x] Messkette in C++: `--match` (Spot-Suche gegen fremde Engines, A/B gegen die
      eigene Vorgängerversion), `--tune` (Texel mit Freigabesperre), `tools/nightly.ps1`
- [x] PeSTO-Basis als PST (siehe M5)
- [x] Lazy SMP: `setoption name Threads value N` (1..64, Standard 1). Helfer
      suchen dieselbe Stellung und füllen nur die gemeinsame Transpositionstabelle;
      Killer/History/Key-Verlauf sind pro Thread (`thread_local`).
      Gemessen (3 s, gleiche Stellung): 1 Thread 0,63 M Nodes / 558k Nps / Tiefe 14;
      4 Threads 2,05 M / 1,34 M Nps / Tiefe 16; 8 Threads 3,33 M / 1,63 M Nps.
      4 Threads ist hier das Optimum. Werkzeug: `tools/smp_test.ps1`.
- [x] Lizenzprüfung der Vorbilder: Arasan ist MIT (nutzbar), Ethereal/Stockfish
      sind GPL-3 (nur Ideen) - Details in `docs/NOTES.md`, Quellen in `ref/`
- [ ] Größerer Tuning-Datensatz (`--tune 500 8 5`, 10⁴-10⁵ Stellungen) und
      Tuning in Stufen: erst Material, dann PST, dann der Rest
- [x] Materialwerte an den Konsens angepasst (Arasan 23.4, PeSTO, übliche
      HCE-Faustregel): Springer 395/377, Läufer 405/400, Turm 595/620,
      Dame 1225/1180 - vorher deutlich flacher (320/330/500/950)
- [x] Mobilität auf die getunte Form umgestellt (vier sichere Züge = neutral,
      darunter Strafe) statt linear durch den Nullpunkt
- [x] Turm auf der 7. Reihe herunternehmen (PeSTO enthält den Bonus schon;
      Arasan bewertet ihn mit 0 (mg) / 9 (eg)) - von 30 auf 8 cp
- [x] Freibauern nach Reihe staffeln (Arasan: 7. Reihe 174/216 gegenüber
      unseren flachen 20/40) - Staffelung 33/100 bzw. 23/55/100 Prozent
- [x] Eingesperrte Leichtfigur bestrafen (Arasan: eingesperrter Läufer -147 cp),
      über die Mobilitätsform statt über ein neues Gewicht
- [ ] Königssicherheit auf gewichtete Angriffseinheiten umbauen
      (Bauer/Minor/Turm/Dame mit eigenen Faktoren, plus Deckungstabelle
      `KING_COVER[6][4]`) - Vorbild in `ref/arasan-v23.4.0/params.cpp`
- [ ] Materialabhängige Skalierung des Mittel-/Endspiel-Übergangs
      (`MATERIAL_SCALE` in Arasans `Scores::set`) statt der reinen Figurenzahl
- [ ] Suchparameter tunen (LMR/LMP/Futility-Margins, Null-Move, Aspiration) -
      das ist der beste Elo-pro-Aufwand-Hebel nach der Bewertung
- [ ] Lazy SMP (`Threads`-Option): ~50 Zeilen, bei fester Zeit grob +60..100 Elo
- [ ] Make/Unmake statt Copy-Make und inkrementelles Material/PST
- [ ] Stile als Deltas, damit die 215 Bots die Tuning-Gewinne erben
- [ ] Eröffnungsbuch, menschliche Bedenkzeiten
- [ ] Elo-Eichung der eigenen 700-2800-Skala gegen die Messung

## M6 - Auslieferung (offen)

- [x] `dist\virgin-<abi>` für die eigene App (`build_android.ps1 -Abi all -Dist`)
- [ ] App-Dokumentation: Katalog lesen, Bot starten, Gewichte live setzen (in Arbeit)
- [x] Nachtlauf (`tools/nightly.ps1`): Build + Daten + Selbsttest + Selfplay

## Bekannte Einschränkungen (Stand M3/M4)

* Bewertung ist noch schlank: nur Material, PST-Formen und Tempo. Königssicherheit,
  Mobilität, Bauernstruktur usw. sind registriert und von außen einstellbar,
  wirken aber erst ab M5. Im Selfplay sieht man das an Randbauernzügen
  (h2h4, a7a5) - die kommen vom Bauern-PST ohne Strukturwissen.
* Copy-Make statt Make/Unmake: sicher, aber etwa 10-20 % langsamer als nötig.
  Optimierung erst, wenn Profiling es fordert.
* Nur ein Suchthread (kein Lazy-SMP); `Threads` wird nicht angeboten.
* Elo-Werte sind nominell und noch nicht gegen Referenzengines kalibriert.

## Vorgemerkt: M5Chess S3 / Bauerino (04.11.2026)

- [ ] Touch-AMOLED-Version mit Stellungseditor, voller Analyse, dauerhaftem Speicher für die letzten 20 Partien, Partie-Viewer und PGN-Export/Teilen. Passende bestehende Funktionen aus eChessTactics übernehmen.
- Details: [M5CHESS_S3_FUTURE.md](M5CHESS_S3_FUTURE.md). Noch keine Umsetzung gestartet.
