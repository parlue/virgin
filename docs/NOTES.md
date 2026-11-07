# Notizen / Entscheidungsprotokoll

Dieses Dokument ist das Merkheft des Projekts. Hier stehen die getroffenen
Entscheidungen, der aktuelle Stand und was als Nächstes dran ist.

## Auftrag (vom Nutzer)

* UCI-kompatible Schachengine in C++, Zielstärke ca. 2800 Elo
  ("selbst wenn wir 2500 erreichen wäre das toll").
* **Leicht aggressiver Spielstil** ist das wichtigste Kriterium.
* **Alle Gewichtungen** (Königssicherheit, offene Linien, ...) müssen von
  außen beeinflussbar sein, denn daraus werden **Bots** gebaut:
  pro Stärkestufe 5 Stile, von "ängstlich" bis "mutig wie ein Stier".
* Stärkestufen im 50er-Raster (700, 750, ... ), zusätzlich freie Ganzzahl.
* Kompilierbar unter **Windows und Android**, Binary möglichst klein.
* Test erfolgt in einer eigenen App des Nutzers.
* Projektordner: `C:\Users\DirkSommerfeld\Documents\Projects\virgin`.

## Entscheidungen

| # | Entscheidung | Begründung |
|---|---|---|
| 1 | Handgebaute Evaluation (HCE), **kein** NNUE | Nur so sind alle Gewichte einzeln von außen steuerbar (Kernanforderung). Beleg, dass 2800+ damit möglich ist: Ethereal 12.75 (letzte reine HCE-Version) = 3454 CCRL 40/15. |
| 2 | Zwei Achsen: **Stärke** (Elo) × **Stil** (Mut 1-5) | Ergibt 43 Elo-Stufen × 5 Stile = **215 Bots**. |
| 3 | Stilachse = Mut/Tapferkeit: Furchtsam, Vorsichtig, Ausgeglichen, Angriffslustig, Stier | Entspricht dem Wunsch "ängstlich bis mutig wie ein Stier". `Boldness` (spin 1-5) erlaubt zusätzlich Zwischenstufen. |
| 4 | Stil wirkt **immer zu 100 %**, die Wissen-Skalierung der Elo-Stufe wirkt nur auf die Basiskenntnis | Sonst wäre "Stier 700" nicht mehr als Stier erkennbar. |
| 5 | Stil wirkt nicht nur auf die Bewertung, sondern auch auf die **Suche** (Zufallsfenster `Misc.MoveNoise`, Wiederholungsvermeidung, Opfertoleranz) | Ein Stil, der nur Zahlen verschiebt, fühlt sich nicht anders an. |
| 6 | Elo-Handicap nach dem MadChess-Muster: Wissen in %, Suchtiefe, Node-Limit, Fehler-/Patzerquote | Erprobtes Verfahren für den Bereich bis 700 Elo herunter (Stockfish kann erst ab 1320). |
| 7 | Ein Gewichtssystem mit **90 registrierten Parametern**, jeder als UCI-Option (`Material.Pawn.Mg`, `Aggression.KingAttack`, ...) plus Datei `data/weights_default.txt` + `ReloadWeights` | "Alle Gewichtungen von außen beeinflussbar" - per GUI-Slider, per Datei oder per App. |
| 8 | Datenformate für die Engine bewusst einfach (Text, `Name = Wert`, `[section]`), JSON nur für die App | Kein JSON-Parser im Binary -> kleiner, robust, keine Abhängigkeiten. |
| 9 | Kein Code aus GPL-Engines kopiert; nur Ideen/Algorithmen übernommen | Eigene Lizenz (MIT) bleibt möglich. |
| 10 | Copy-Make statt Make/Unmake in Meilenstein 1 | Minimiert Fehlerquellen; Messung: 14-25 M Nodes/s, für 2800 Elo ausreichend. Optimierung erst wenn Profiling es fordert. |
| 11 | Bot-Katalog wird aus 5 Stilprofilen + 43 Elo-Profilen **generiert**, nicht als 215 Einzeltabellen gespeichert | Wartbar; die App liest `data/bots.json`. |
| 12 | Ein einziger Randomisierungspunkt im Root (`Misc.MoveNoise`) | Lehre aus Stockfish PR #3769: doppelte Randomisierung zerstört die monotone Elo-Skalierung. |
| 13 | **Keine Fremd-GUI als Pflichtschritt**: die Engine ist ein UCI-Prozess, die eigene App ist der Host | Der Test läuft über die App bzw. über die Engine-Modi `--selftest` und `--selfplay`. DroidFish/Chess for Android nur noch als optionale Gegenprobe bei der Fehlersuche (`docs/BUILD_ANDROID.md`). |
| 14 | **Alles in C++**: Daten, Tests und Selfplay sind Engine-Modi (`--gen-data`, `--selftest`, `--selfplay`) | Keine Hilfssprachen im Projekt. Die Stil-/Elo-Daten liegen in `src/botdata.cpp`, werden von der Engine erzeugt und in M4 direkt angewendet. |
| 15 | **Eine Datei genügt**: die Engine braucht keine Datendateien | Gewichte, Stile, Elo-Profil und Katalog stecken in der Binary; `data/` ist optional. Ausgabe für die App: `dist\virgin-<abi>` (`build_android.ps1 -Abi all -Dist`). |

## Stand (Meilenstein 1-5)

Erledigt:

* CMake-Build für Windows (MSVC 2022) **und** Android (NDK 27) - je ein Skript.
* Board, Bitboards (Magic Bitboards), Movegen, Make/Copy, FEN, Zobrist, Perft.
* UCI-Frontend inkl. `d`, `eval`, `perft`, `bench`, `compiler`, `sanity`,
  `weights`, `listbots`; Suche im eigenen Thread (`stop`/`ponderhit`).
* Suche: iterative Vertiefung mit Aspiration Windows, PVS, Transpositionstabelle,
  Quiescence + SEE, Null-Move, LMR/LMP/Futility/Razoring, Killer, Countermove,
  History, Wiederholungs- und 50-Züge-Erkennung, `Misc.Contempt` als
  Remis-Bewertung im Suchbaum.
* Bewertung: 13 Termgruppen (Material, PST, Mobilität, Königssicherheit,
  Bauernstruktur, Türme, Figuren, Bedrohungen, Raum, Initiative, Endspiel,
  Aggression, Komplexität) plus Remis-Skalierung und Lazy Evaluation.
* Gewichtssystem: 91 Parameter, alle als UCI-Option + Datei + Reload.
* Bots: 5 Stile, 43 Elo-Stufen, 215 Bots; `Bot`/`Style`/`Boldness`/`UCI_Elo`
  setzen Stilgewichte, Wissens-Skalierung und Such-Handicap.
* Daten + Werkzeuge als Engine-Modi: `--gen-data`, `--selftest` (43 Prüfungen),
  `--selfplay`.
* Auslieferung: `dist\virgin-arm64-v8a`, `-armeabi-v7a`, `-x86_64`.

Offen (nächste Schritte):

* **Texel-Tuning**: die Termgewichte sind geschätzt, nicht kalibriert. Das ist
  der größte offene Hebel für die Spielstärke.
* Eröffnungsbuch und menschliches Zeitverhalten.
* Elo-Kalibrierung: Werkzeug ist fertig (`virgin --match <engine.exe> ...` bzw.
  `run_match.cmd`), erste Messung gegen Stockfish 18 siehe Messwerttabelle.

## Automatisierung (vorhanden)

| Einstieg | Wirkung |
|---|---|
| `run_all.cmd` | Build -> Daten (`--gen-data`) -> Selbsttest (`--selftest`), ein Befehl mit Exit-Code |
| `build_win.ps1` | MSVC-Build inkl. Größenangabe und Perft-Smoke-Test |
| `build_android.ps1` | NDK-Build (arm64/armv7/x86_64, PIE oder `-Static`) inkl. `llvm-readelf`-Selbstprüfung; `-Dist` kopiert die Binaries nach `dist\` |
| `virgin --gen-data [dir]` | erzeugt alle Datendateien aus der C++-Registry und `src/botdata.cpp` |
| `virgin --selftest [dir]` | eingebauter Testlauf (43 Prüfungen), Exit-Code für Skripte/CI |
| `virgin --selfplay a b moves depth` | Partie zwischen zwei Bot-Profilen, Log nach `reports/` |
| `virgin --match <engine.exe> [startElo] [step] [games] [ms/Zug] [maxElo] [bot] [logDir] [levels\|up] [gegnerMs] [weightsFile]` | Wettkampf gegen eine fremde UCI-Engine (z. B. Stockfish 18 mit `UCI_LimitStrength`): Spot-Suche über Elo **und** Gegner-Bedenkzeit, Abbruch nicht mehr gewinnbarer Stufen, PGN nach `reports/`; mit `weightsFile` wird eine getunte Gewichtsdatei über den Stil geladen |
| `virgin --tune [games] [depth] [passes] [outFile] [maxDeltaPercent]` | Texel-Tuning: Selfplay-Datensatz, K-Scan, Koordinatenabstieg mit Train/Validation-Split; schreibt die Gewichte nur bei besserem Validierungsfehler (Exit 4 = verworfen) |
| `run_match.cmd` | fertiger Aufruf gegen Stockfish 18 (Standard: 1320, Schritt 50, 6 Partien, 1000 ms/Zug, Gegnerzeit frei) |
| `tools/nightly.ps1` | Nachtlauf: baut, erzeugt Daten, testet, spielt, schreibt Report |

Alles ist in C++ implementiert - es gibt keine Hilfssprachen im Projekt. Die
PowerShell-Skripte sind nur Start-/Bau-Orchestrierung für Windows (MSVC- und
NDK-Aufruf), keine Programmlogik.


## Belegte Messwerte (Stand jetzt)

| Prüfung | Ergebnis |
|---|---|
| Perft startpos d5 | 4.865.609 korrekt |
| Perft Kiwipete d4 | 4.085.603 korrekt |
| Perft Endspiel/Position 4/5/6 | korrekt (siehe `virgin --selftest`) |
| Perft-Geschwindigkeit | 8-22 M Nodes/s |
| Suchgeschwindigkeit | 4,99 M Nodes bei Tiefe 12 (607k N/s, `--bench 12`, volle Bewertung) |
| Suchtiefe im Mittelspiel | 10-12 |
| Taktik | Matt-in-1 gefunden (score 30999), hängende Dame geschlagen |
| Elo-Handicap | v700 spielt Tiefe 2, v2000 Tiefe 10 (Matt in 9 gefunden) |
| Zobrist-Konsistenz | 200 Züge geprüft, Schlüssel stabil |
| Windows-Binary | 305.152 Bytes (298 KB), MSVC 19.43 |
| Android arm64-v8a PIE | 621 KB, ELF DYN, nur libc/libm/libdl |
| Android arm64-v8a statisch | 780 KB, ELF EXEC, keine Abhängigkeiten |
| Android armeabi-v7a PIE | 387 KB, ELF DYN |
| Android armeabi-v7a statisch | 480 KB |
| Gewichte | 91 Parameter, alle als UCI-Option |
| Bot-Katalog | 215 Bots (43 Elo-Stufen x 5 Stile) |
| Gesamtlauf `virgin --selftest` | RESULT: PASSED (43 Prüfungen) |
| Wettkampf gegen Stockfish 18, `UCI_Elo`-Treppe, 2 Partien/Stufe, 150 ms/Zug | Stufe 2300: +1 =0 -1 (50 %), Stufe 2350: +0 =0 -2 (0 %) -> **gemessen ~2325 Elo** (Klammer 2300..2350), PGN in `reports/` |

## Behobene Fehler (Lernpunkte)

* `init_eval()` rief `refresh_eval_weights()` auf, bevor die Gewichtstabelle
  gefüllt war -> Zugriffsverletzung (0xC0000005). Jetzt initialisiert `main()`
  zuerst `init_weights()`, und `weight_value()` fällt defensiv auf den
  Registrierungswert zurück.
* `-static` war in `CMakeLists.txt` fest verdrahtet, dadurch war der Android-Build
  nicht PIE-fähig. Jetzt über die Option `VIRGIN_STATIC` (Standard: AUS) steuerbar;
  `build_android.ps1` setzt sie nur mit `-Static`.
* Umgeschaltete CMake-Optionen wurden nicht wirksam, weil der Cache alte Werte
  hielt -> Build-Skript nutzt jetzt getrennte Build-Verzeichnisse
  (`build/android-<abi>` und `build/android-<abi>-static`) und `-Clean`.
* Wettkampf-Modus gegen Stockfish: Stockfish spielte immer aus der
  Grundstellung, weil die Position-Zeile `position startpos e2e4 e7e5 ...` hieß.
  Das UCI-Schlüsselwort **`moves`** fehlte; Stockfish verwirft dann alle
  folgenden Tokens. Jetzt: `position startpos moves <zuege>`.
* Wettkampf-Modus: `Position::isLegal()` prüft nur die Königssicherheit und
  unterstellt, dass der Zug pseudo-legal ist. Für Züge aus einer fremden Engine
  ist das zu schwach (ein Zug von einem leeren Feld "gilt" als legal). Fremdzüge
  werden jetzt gegen die Liste aus `generateLegal()` gemappt (`findLegalMove`);
  nicht gefundene Züge sind verloren.
* **Texel-Tuning, erster Versuch verworfen (Overfitting)**: 60 Partien /
  2698 Stellungen senkten den Trainingsfehler um 11 %, im Match gegen Stockfish
  war die Engine damit aber **deutlich schwächer** (33 % statt 67 % gegen
  `UCI_Elo 1320` bei 1 s/Zug). Alle Parameter waren am maximalen Schritt (±16)
  in eine Richtung gelaufen. Lehre: ohne **Validierungssatz** ist Texel-Tuning
  auf kleinen Datensätzen schädlich. Jetzt fest eingebaut: deterministischer
  80/20-Split, Schranke von ±20 % pro Parameter gegenüber dem kompilierten
  Default, und die Ergebnisdatei wird **nur geschrieben, wenn der
  Validierungsfehler besser wird** (sonst Exit 4 und Defaults bleiben).
  Zweiter Versuch: 200 Partien / 8681 Stellungen (6945 Training, 1736
  Validierung), K = 240 cp, Trainingsfehler 0.102819 -> 0.097904 (−4,8 %),
  Validierungsfehler 0.098148 -> 0.094089 (−4,1 %) -> akzeptiert.
* **Wurzel-Suche (schwerster Fehler bisher)**: Nachfolgerzüge wurden mit dem
  Aspirationsfenster `(-beta, -a)` nachgesucht. Ein Zug, dessen wahrer Wert weit
  unter dem Fenster liegt, "fällt hoch" und liefert nur eine **Schranke**; die
  Wurzel hielt diese Schranke für den echten Wert. Folge: Züge, die Material
  wegwerfen, bekamen fast denselben Wert wie der beste Zug und wurden gespielt.
  Beweise: nach `4.Lxc6` statisch −311, die Suche gab für `g5` aber −76 statt
  ≈ −324; `info` meldete `pv c6d5`, `bestmove` war `c6d7`; im Selfplay `11.De4??`
  bei eigener Bewertung +194. **Behoben:** die Wurzel holt für **jeden** Zug
  einen exakten Wert (Nachsuche mit vollem Fenster, auch für den ersten Zug).

* **Toter Code in der Remis-Skalierung**: `oppositeBishopsOnly()` konnte nie
  greifen, weil `insufficientMaterial()` (keine Bauern/Türme/Damen, höchstens
  ein Leichtstein je Seite) **dieselben** Stellungen vorher abfängt - und
  ungleichfarbige Läufer bedeuten zwangsläufig genau einen Läufer je Seite.
  Das Gewicht `Drawish.OppositeBishops` (Standard 60) war damit wirkungslos.
  **Behoben:** die spezielle Regel wird jetzt zuerst geprüft. Der Selbsttest
  hat den Fehler aufgedeckt, weil er nach dem PST-Wechsel plötzlich anschlug -
  und dabei zeigte, dass die alte Prüfstellung `8/5pk1/8/8/8/2b5/5PK1/6B1`
  zwei Läufer auf **derselben** Feldfarbe hat (g1 und c3 sind beide dunkel).
  Neuer Test: `2b1k3/8/8/8/8/8/8/2B1K3`, und geprüft wird skaliert gegen
  unskaliert (6 gegen 10), also unabhängig von der PST-Kalibrierung.

* **Rochade in einer UCI-Zugliste desynchronisierte das Brett (App-relevant)**:
  `position startpos moves ... e1g1` wurde mit `parse_uci_move()` gelesen - das
  kennt für "e1g1" nur einen Königszug (`MF_NORMAL`) - und dann angewendet: der
  **Turm blieb auf h1 stehen**. Beweis: nach
  `position startpos moves e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1` meldete
  `d` die FEN `r1bqkb1r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ2KR b kq - 3 5`
  (König g1, Turm h1). Jede GUI oder App, die Züge als Liste schickt, bringt die
  Engine damit aus dem Tritt; im A/B-Kampf endeten 66 Züge in 38 Partien mit
  "opponent played an illegal move". Ursache ist dieselbe Fehlerklasse wie im
  Match-Modus: `Position::isLegal()` prüft nur die Königssicherheit, ein
  Königszug e1→c1 "gilt" damit als legal, und `applyMove()` richtet sich nach
  den Flags. **Behoben:** neuer gemeinsamer Helfer `find_legal_move(pos, text)`
  in `movegen.cpp` löst den Text gegen die Liste aus `generateLegal()` auf, damit
  Rochade und En passant ihre echten Flags bekommen. Genutzt von `uci.cpp`
  (Position-Kommando) und `match.cpp` (Eröffnungszüge). Zwei neue Selbsttests
  prüfen genau das (43 Prüfungen).

### Stand der Messung (ehrlich)

Der A/B-Lauf gegen `build/win/virgin_base.exe` (beide 100 ms/Zug, Elo 2800 =
kein Handicap, 8 Eröffnungen mit Farbtausch) ist **noch nicht aussagekräftig**:
nach 10 Partien steht es 4:6, das sind -70 ± 110 Elo, also nichts. Der Lauf
braucht Stunden (`reports/ab-pesto2.log`). Für belastbare Zahlen in nützlicher
Zeit: kleinere Bedenkzeit (20-30 ms/Zug) für mehr Partien pro Stunde.

**Offener Verdacht für die nächste Messung:** die PeSTO-Tabellen werden
*additiv* zu den eigenen Termen benutzt, und PeSTO kennt viele davon schon
(Turm auf der 7. Reihe ≈ +80, Bauernschild, Bauernfortschritt im Endspiel).
Unsere Terme (`Rooks.OnSeventh`, `Pst.RookRank7`, `KingSafety.Shield*`,
`Pawns.PassedEg`) zählen also doppelt. Testbar **ohne Neubau**: Gewichtsdatei
mit `Pst.RookRank7 0`, `Pst.KingMgShelter 0`, `Pst.PawnEgAdvance` klein und
diese als 13. Argument an `--match` geben.

## Regeln für die Zusammenarbeit (wichtig)

* **`dist\meine-engine\` gehört dem Nutzer.** Dateien dort werden von der
  Entwicklung **nie** überschrieben, verschoben oder gelöscht - dort laufen
  Testläufe, die eine feste Engine-Version brauchen. Neue Versionen bekommen
  einen **neuen Dateinamen** daneben (z. B. `virgin-stand-2026-10-30.exe`).
  Siehe `dist/meine-engine/FINGER-WEG.txt`.
* `dist\virgin-test\` und `dist\vergleich\` dürfen aktualisiert werden, aber
  ebenfalls besser als neue Datei statt als Überschreibung.
* Während einer laufenden Messung die Maschine nicht anderweitig belasten.

## Messung: Durchsatz und Verdikt (Werkzeugstand)

Eine wichtige Lehre aus dieser Runde: **das Werkzeug war nie zu langsam, die
Beobachtung war falsch.** Nachgemessen mit `tools/match_rate.ps1`:

* 20 × (`position` + `go movetime 100` + `isready`) an die Engine: **2877 ms,
  also 144 ms pro Zug** (100 ms Suche + 44 ms Prozess-/Pipe-Overhead) - die
  Engine selbst ist also in Ordnung.
* Ein laufender A/B-Kampf (100 ms/Zug): **12,1 Züge/s = 0,08 s/Zug**, das sind
  **~3,4 Partien pro Minute**. 60 Partien brauchen also ~17 Minuten, 100 Partien
  ~30 Minuten.

Drei Dinge hatten den falschen Eindruck "das dauert Stunden" erzeugt:

1. Keine Zeitstempel im Log - ich habe die verstrichene Zeit geschätzt statt
   gemessen (der Lauf war 14 Minuten alt, nicht eine Stunde).
2. Meine eigenen Werkzeugaufrufe (PowerShell, Builds, VS Code) konkurrieren mit
   den Engines um die CPU. Während einer Messung sollte man die Maschine in Ruhe
   lassen - sonst misst man sich selbst.
3. Die Partie-Zeilen im Log kommen blockgepuffert, der Zähler hinkt hinterher.

Deshalb schreibt der Match jetzt **selbst**, was man zum Beurteilen braucht:

```text
  game 12/100  [03:41]  we play black, opening 6 -> LOSS (188 plies, adjudicated: material down)
  ...
 overall: +28 =5 -25   58 games   score 52.6 %
 verdict: +18 Elo vs 50 % (95 % confidence +/- 52)   in 17.0 min   (3.4 games/min)
```

Faustregel für die Aussagekraft: die 95 %-Schranke ist ungefähr
`680 / sqrt(Partien)` Elo. 100 Partien lösen also nur rund ±70 Elo auf, 500
Partien ±30, 1000 Partien ±21. Kleine Unterschiede brauchen viele Partien -
das ist der eigentliche Grund, warum Bewertungsarbeit langsam zu validieren ist.

### Bisherige A/B-Ergebnisse (alle gegen die eigene Vorgängerversion, 100 ms/Zug)

| Schritt | Partien | Ergebnis | Verdikt |
|---|---|---|---|
| PeSTO-PSTs statt handgemachter Formen | 14 | 5:8 | keine Aussage (±180 Elo) |
| Material + Mobilität nach Arasan | 58 | 28:25:5 = 52,6 % | +18 Elo (±90) |
| Gesamtsession vs. Vor-PeSTO | 34 (abgebrochen) | 13:16:5 = 45,6 % | −30 Elo (±117) |
| Freibauern-Staffelung + Turm 7. + eingesperrte Figur | läuft | | |

Ehrliche Lesart: **keiner dieser Schritte ist als Gewinn belegt.** Die
Stichproben sind zu klein (siehe Faustregel oben) und die Effekte zu klein. Was
dagegen belegt ist: die Korrektheitsfehler dieser Session (Wurzel-Suche,
Rochade/En passant in Zuglisten, toter Remis-Code) - die haben ganze Partien
entschieden und waren im Log direkt sichtbar (66 illegale Züge in 38 Partien).

Konsequenz für die weitere Arbeit: erst **große** Hebel angehen, die den
Rauschpegel sicher überschreiten (Lazy SMP, Make/Unmake, Suchparameter), und
kleine Bewertungsdeltas nur über größere Läufe (500+ Partien) beurteilen.

## Lazy SMP: mehr Tiefe durch mehr Threads

Seit dieser Runde hat die Engine `setoption name Threads value N` (1..64,
Standard 1). Die Helfer-Threads suchen **dieselbe** Stellung und schreiben nur in
die gemeinsame Transpositionstabelle - wer eine Stellung zuerst sieht, erledigt
die Arbeit für die anderen. Killer, History, Countermoves und der Zobrist-Verlauf
sind `thread_local`, nur die TT wird geteilt. Nur der Haupt-Thread spricht mit
der GUI und beachtet die weiche Zeitgrenze; die Helfer stoppen an der harten.

Gemessen mit `tools/smp_test.ps1` (gleiche Stellung, 3 s):

| Threads | letzte fertige Tiefe | Nodes | Nodes/s |
|---|---|---|---|
| 1 | 14 | 0,63 M | 558 k |
| 2 | 15 | 1,10 M | 855 k |
| 4 | **16** | 2,05 M | **1,34 M** |
| 8 | 14 | 3,33 M | 1,63 M |

Auf dieser Maschine (4 Kerne) ist **4** das Optimum; 8 Threads bringen kaum mehr
Nps und keine Tiefe. Auf einem Handy gilt dasselbe Prinzip mit 2-4 Threads -
mehr kostet nur Akku und Wärme.

## Was wir von Arasan gelernt haben (ohne Code zu übernehmen)

Arasan 23.4 ist die letzte Version mit handgeschriebener Bewertung und steht
unter MIT (`ref/arasan-v23.4.0/`). Wir haben **nur gelesen und nachgebaut**, in
eigener Struktur und eigenen Worten. Arasan rechnet mit **128 = ein Bauer**;
unten umgerechnet auf unsere Skala (100 = ein Bauer).

| Term | Arasan 23.4 (÷1,28) | unsere alten Werte | Konsequenz |
|---|---|---|---|
| Springer | 394 / 377 | 320 / 300 | angehoben |
| Läufer | 406 / 401 | 330 / 310 | angehoben |
| Turm | 594 / 620 | 500 / 550 | angehoben |
| Dame | 1224 / 1178 | 950 / 980 | stark angehoben |
| Bauer (eg) | 107 | 120 | leicht gesenkt |
| Mobilität Springer (0→8 Züge) | −66 … +12 | 0 … +32 | Form geändert |
| Mobilität Läufer (0→14) | −45 … +30 | 0 … +65 | Form geändert |
| Mobilität Turm (0→14) | −55 … +14 | 0 … +28 | Form geändert |
| Turm auf der 7. Reihe | 0 (mg) / 9 (eg) | 30 (PST + Term) | offener Punkt |
| Läuferpaar | 33 / 54 | 30 | passt |
| Freibauer 7. Reihe | 174 / 216 | 20 / 40 flach | offener Punkt |
| eingesperrter Läufer | −147 | fehlt | offener Punkt |

Umgesetzt:

1. **Materialwerte** an den Konsens angepasst (`src/weights.cpp`). Drei
   unabhängige Quellen (Arasan, PeSTO, die übliche HCE-Faustregel
   Springer≈3,8 Bauer / Dame≈12,2 Bauer) sagen dasselbe, unsere Schätzung war
   der Ausreißer. Der Texel-Tuner hatte vorher schon in genau diese Richtung
   gezogen (Bauer −16, Turm −16, Dame +16).
2. **Mobilität** von einer Geraden durch den Nullpunkt auf die getunte Form
   umgestellt: vier sichere Züge = neutral, darunter Strafe, darüber nur
   flacher Zuwachs (`src/eval.cpp`). Die Gewichte bleiben pro Zug einstellbar.
3. **Freibauern nach Reihe gestaffelt** (5. Reihe 33 %, 6. Reihe 100 % im
   Mittelspiel; 23 %, 55 %, 100 % im Endspiel) statt einer Geraden; die Gewichte
   bedeuten jetzt den 7.-Reihen-Bonus (100/100 statt 20/40 flach).
4. **Turm auf der 7. Reihe** von 30 auf 8 cp gesenkt (PeSTO hat den Bonus schon,
   Arasan bewertet ihn mit 0/9).
5. **Eingesperrte Leichtfigur** bestraft (vierfaches Mobilitätsgewicht), ohne
   neues Gewicht - Arasans eingesperrter Läufer liegt bei −150 cp.

Offen (in `docs/ROADMAP.md`): gewichtete Angriffseinheiten für die
Königssicherheit (`PAWN_ATTACK_FACTOR`, `MINOR_ATTACK_FACTOR`,
`ROOK_ATTACK_FACTOR`, `QUEEN_ATTACK_FACTOR` plus `KING_COVER[6][4]`-Tabelle)
und die materialabhängige Skalierung des Mittel-/Endspiel-Übergangs
(`MATERIAL_SCALE`, bei uns bisher nur die Figurenzahl als Phase).

Baseline für die Messung dieses Schritts: `build/win/virgin_base2.exe`
(PeSTO-Basis, alte Material- und Mobilitätswerte).

## Stärke: Herkunft der Bewertung (PeSTO-Basis)

Die PST-Formen der ersten Meilensteine waren **geschätzt** (Bauer linear
+3 cp je Reihe) und damit deutlich schwächer als veröffentlichte, getunte
Tabellen. Seit diesem Stand baut `build_pst_tables()` auf den **PeSTO-Tabellen**
auf (Ronald Friederich, Engine RofChade/PeSTO, veröffentlicht auf der
Chess Programming Wiki, Tauziehen per Texel-Tuning):

* `src/pst_pesto.h` wird von `tools/extract_pesto.ps1` aus der gespeicherten
  Wiki-Seite (`ref/pesto.html`) erzeugt - keine 768 Zahlen von Hand.
* PeSTO indiziert `a8 = 0`, diese Engine `a1 = 0`; die Tabellen werden beim
  Aufbau mit `square ^ 56` gedreht.
* PeSTO rechnet mit Bauer = 82/94, diese Engine mit 100; die Tabellen werden
  mit `100/88` skaliert, damit das Verhältnis Figur/Bauer erhalten bleibt.
* Die 12 registrierten `Pst.*`-Gewichte bleiben erhalten und werden als
  **Delta** auf die Basis addiert: Stile, Bots und Tuner funktionieren weiter,
  aber die Basisform kommt jetzt aus getunten Daten.

Warum das mehr bringt als "mehr Tiefe": die PeSTO-Tabellen enthalten Wissen,
das die alte Form gar nicht ausdrücken konnte (Turm auf der 7. Reihe ≈ +80,
Bauer auf der 7. Reihe im Endspiel +178, Läufer auf g1 −39 gegen c3 +15).
Fehlbewertungen dieser Größe kosten Partien unabhängig von der Suchtiefe.

### Lizenzprüfung der Vorbilder (Stand: dieses Vorgehen)

| Engine | Lizenz | Verwendbar? |
|---|---|---|
| Arasan (Jon Dart, ≤ 26.1) | **MIT/Expat** (Copyright 1994-2026) | ja, auch Codeübernahme; ab v26 NNUE-only, die klassische Eval liegt in den Tags ≤ 23.x |
| PeSTO-Tabellen (RofChade) | veröffentlichte Daten (Wiki CC BY-SA 3.0) | ja, als Daten mit Quellenangabe |
| Ethereal (Andrew Grant) | GPL-3.0 | nein (Code), nur Ideen |
| Stockfish | GPL-3.0 | nein (Code), nur Ideen |

Die Referenzquellen liegen unkompiliert unter `ref/` (siehe `ref/README.md`).

### Messwerkzeug: A/B gegen das eigene Vorgänger-Binary

Für Bewertungsänderungen ist der Spot-Lauf gegen Stockfish zu grob und zu
langsam. `--match` kann **jedes** UCI-Programm als Gegner starten, also auch
die eigene vorherige Version:

```powershell
# Baseline sichern, dann neu bauen
Copy-Item build\win\virgin.exe build\win\virgin_base.exe
# 100 Partien, beide Seiten 100 ms/Zug, volle Stärke (Elo 2800 = kein Handicap)
build\win\virgin.exe --match build\win\virgin_base.exe 2800 50 100 100 2800 v2800-ausgeglichen reports 1 100
```

Beide Engines laufen mit identischem Code und identischer Bedenkzeit; der
Unterschied ist dann praktisch nur die Bewertung (gepaarte Stichprobe,
8 Eröffnungen mit Farbtausch). Das ist deutlich rauschärmer als zwei getrennte
Spot-Läufe und liefert direkt ein Elo-Delta.

### Werkzeuge

* `tools/gate.ps1` - Build + `--gen-data` + `--selftest` + `--bench` in einem
  Lauf, Ausgabe als lesbare Datei (`reports/gate.txt`, `GATE GREEN`/`GATE FAILED`).
* `tools/extract_pesto.ps1` - erzeugt `src/pst_pesto.h` aus `ref/pesto.html`.

  (Beleg: nach `1.e4 e5 2.Sf3 Sc6 3.Lb5 a6 4.Lxc6` spielte die Engine `4...g5`
  mit Bewertung −76 statt `4...bxc6` mit −71, obwohl `g5` ≈ −324 ist; im Selfplay
  folgte `11.De4??` nach einer Bewertung von +194). Zusätzlich widersprachen sich
  `pv` und `bestmove` in der `info`-Zeile. Fix: **jeder** Wurzelzug wird exakt
  bewertet (Nachsuche mit vollem Fenster, auch für den ersten Zug), erst danach
  entscheidet der Vergleich. Erkannt an den Partien gegen Stockfish: 8 von 9
  Niederlagen durch Matt, obwohl die eigene Bewertung vorher +1,5 bis +3,0 zeigte.
* Wettkampf-Modus, Abstiegslogik: die Elo-Suche nutzte `lo < hi` als Test für
  "Untergrenze bekannt". Da `lo == 0` auch "noch unbekannt" bedeutet, sprang die
  Suche nach vier Niederlagen auf Stufe 2300 direkt auf `(0+2300)/2 = 1150`,
  wurde auf Stockfishs Untergrenze 1320 geklemmt und blieb dort für alle
  restlichen Stufen stehen. Jetzt gibt es getrennte `haveLo`/`haveHi`-Merker,
  einen Abbruch bei "nächste Stufe == aktuelle Stufe" und eine zweite Achse:
  wenn die Elo-Untergrenze erreicht ist und wir trotzdem verlieren, wird die
  **Bedenkzeit des Gegners halbiert** (1000 -> 500 -> ... -> 30 ms).
* Wettkampf-Modus: `std::min`/`std::max` in `match.cpp` kollidierten mit den
  `min`/`max`-Makros aus `windows.h` (C2059/C2589). Ersetzt durch explizite
  Vergleiche - damit bleibt `match.cpp` ohne `NOMINMAX`-Sonderfall portabel.


## Stand 31.10.2026: Virgin 0.3.0 nach Carballo-Partie

Aktueller Kandidat: dist/virgin-0.3.0-20261031, Details in docs/STRENGTH_0.3.0.md.
Ziel des Nutzers: Carballo (ungefaehr 2200 Elo) bei 1 s/Zug klar schlagen. Noch nicht nachgewiesen. Keine Matches gegen andere Engines starten; diese uebernimmt der Nutzer selbst. App-Auswahl und Import ebenfalls beim Nutzer belassen.
0.3.0 erweitert die Koenigszone um vorgelagerte Einbruchsfelder, verbessert Bauernschild und Schachpotenzial, begrenzt Opfertoleranz, erhaelt volle Material-/Defensivkenntnis im Angriffsstil und erzeugt taktische Zuege direkt. 75 Windows-Selbstpruefungen und UCI-Prozesstest bestanden. Android fuer alle drei ABIs gebaut und ELF geprueft; kein Geraetetest. Kritische Partiestellungen mit 1 s und 1/4 Threads vor/nachher untersucht, Protokolle liegen im Release. h4/g3 treten bei einem Thread im Angriffsstil weiterhin auf; keinen geloesten Taktikfall oder Elo-Gewinn behaupten. Kein Matchresultat vorhanden.
Die vollstaendige Nutzerpartie liegt als PGN im Release. 26.Sd5 Txd5 27.exd5 gewinnt die Qualitaet zurueck. Vor einem Koenig g1 muss auch ein Einbruch auf h3 beruecksichtigt werden. Die alte sanityCheck-EP-Richtung war falsch; Zugerzeugung war korrekt.
Vorversion und Quellen gesichert unter backups/before-0.3-20261031-124457. Vorhandene dist-Ordner, insbesondere meine-engine, unveraendert lassen.


## Stand 31.10.2026: Virgin 0.4.0 nach zweiter Nutzerpartie

Aktueller Kandidat: dist/virgin-0.4.0-20261031; Details docs/STRENGTH_0.4.0.md.
Historische Zweifachwiederholung wird nicht mehr pauschal als Remis angenommen; Suchzyklen und echte Dreifachwiederholung bleiben erkannt. UCI-Historie enthaelt jetzt die Anfangsstellung. Bewertung unterscheidet geometrische Angriffe von wegen absoluter Fesselung nicht nutzbaren Rueckschlaegen. Keine neue Gewichtsabstimmung.
Die zweite PGN hat 63 Halbzuege, Schlussstellung nur nach Halbzug 59 und 63. Beide Turmpaare bleiben erhalten, Weiss hat zwei Bauern weniger. Grund fuer das Spielende in der Oberflaeche noch ungeklaert; Virgin erklaert per UCI selbst kein Remis. Ob App gekuerzt/automatisch beendet/vereinbart, beim Nutzer offen. Kein Elo-Gewinn nachgewiesen; Gegnerpartien weiterhin beim Nutzer. Windows-Selbsttest und UCI-Prozesstest bestanden; Android fuer drei ABIs gebaut, kein Geraetetest.
Vorherige Dateien und dist/meine-engine nicht ueberschreiben. Backup before-0.4-20261031-130611.

Nachtrag: Nutzer bestaetigt automatisches Remisende durch die Oberflaeche. Bei vollstaendiger PGN wurde also bereits nach Zweifachwiederholung abgebrochen; Oberflaechenregel pruefen. Hier keine App geaendert. Bericht 0.4.0 aktualisiert.


## Stand 31.10.2026: Virgin 0.5.0 Suchalgorithmen

Neuer Kandidat dist/virgin-0.5.0-20261031, Details docs/SEARCH_0.5.0.md. PVS/Aspiration, konsistente legale SEE, selektive Schach-/Bauerndrohungen, Vierer-TT mit Blattcache, kompakte Fortsetzungshistorie und gebuendelte Knotenzaehlung. 101 Selbstpruefungen, UCI-Smoke und 319 unabhaengige SEE-Faelle bestanden. Android drei ABIs gebaut/ELF geprueft, kein Geraetetest. Kein Elo-Nachweis und keine Gegnerpartien. Nutzer testet selbst; 0.4-Testserie unberuehrt. Ziel Carballo klar schlagen weiter offen. Vorversionen und dist/meine-engine nicht ueberschreiben.


## Stand 31.10.2026: Virgin 0.6.0

Neuer separater Kandidat dist/virgin-0.6.0-20261031. Individuelle Leichtfiguren- und Fluchtfeldbewertung, blockierte/gesicherte Freibauern, keine doppelte Freibauernzaehlung. Nullzug-Verifikationsversuch wegen Tiefenverlust verworfen; finale Suchselektivitaet aus 0.5 beibehalten. 105 Selbstpruefungen, UCI-Smoke, 319 SEE-Orakelfaelle bestanden. Drei Android-ABIs gebaut/ELF geprueft, kein Geraetetest. Kein Elo-Nachweis, keine Gegnerpartien. Neue Remispartie nur als PGN-Kopf vorhanden. Details docs/STRENGTH_0.6.0.md. Vorversionen und meine-engine unveraendert lassen.


## Stand 31.10.2026: Virgin 0.7.0 Adaptive

Nutzer verlangt exakt die gepruefte adaptive Patricia-Zugwahl aus eChessTactics. Quelle gefunden unter Documents/CynusTactics/TalChooser.kt und tal/tal_chooser.py, Werte aus assets/tal/tal_params.json. Portiert als eigene UCI-Checkbox Adaptive, default false. Aktiv: festes Angriffslustig, volle Kenntnis/keine Handicaps, Hash32, Threadsauto; andere Optionen komplett ignoriert, auch ohne Suchabbruch. Aus: vorherige Einstellungen wiederhergestellt. Go-Zeitlimits bleiben verbindlich. Fuenf echte gleich tiefe Kandidaten, originale Zustandsregeln/Schwellen. 680 Entscheider-Vergleiche ohne Unterschied, 111 Selbstpruefungen, UCI-Sitzungstests bestanden. Normalbench zu 0.6 unveraendert. Android drei ABIs gebaut/ELF geprueft, kein Geraetetest. Quellenprojekt unveraendert, Referenzkopien/Hash und Details docs/ADAPTIVE_0.7.0.md. Keine Matches. Vorversionen erhalten.


## 2026-10-31 – 0.7.1: BearChess-Zeitverwaltung und ESP32-S3

BearChess ohne eigene UCI-Einstellungen übergab 120-Minuten-Uhren; 0.7.0 rechnete deshalb minutenlang. Neue Eröffnungs-Zielzeit 1,5 s, harte Grenze 3 s bei Uhrpartien während der ersten vier vollen Züge; stabile klare Entscheidungen, einziger legaler Zug und begrenzt gesuchtes Matt dürfen früher enden. Fehlende bzw. abgelaufene Zeitvorgaben sind endlich; explizites infinite wartet auf stop. UCI-Ausgabe gegen readyok/info-Vermischung geschützt. Direkt in BearChess: 1.e4 e5 in 1,674 s und 2.Sf3 Sf6 in 3,027 s automatisch, ohne stop/Erzwingen. Neuer Eintrag Virgin 0.7.1 installiert; alter virgin-Eintrag und versionierte Alt-Binaries unverändert.

Windows und Android (drei ABIs) unter dist/virgin-0.7.1-20261031. 116 Selbsttests, UCI-Smoke/Adaptive/Zeitsimulation bestanden. Keine Gegnerpartien und kein neuer Elo-Nachweis.

ESP32-S3: drei gebaute ESP-IDF-Profile (ohne PSRAM, Quad-PSRAM, Octal-PSRAM), kleine Angriffstabellen, Flash-Geometrie, dauerhafter einzelner Suchworker, Hash 32/1024 KiB, maximale Suchtiefe 24/64 Halbzüge, Suchstack 96 KiB intern bzw. 256 KiB PSRAM. UCI über natives USB Serial/JTAG. Kompakter Host-Build besteht 116 Selbsttests und sämtliche Adaptive-UCI-Tests, TalChooser-Parität 680/680. Alle Firmware-Images erfolgreich gebaut und esptool-Prüfsumme geprüft, auf Hardware noch nicht ausgeführt/geflasht. Paket dist/virgin-esp32s3-0.7.1-20261031; Details docs/ESP32_S3_0.7.1.md. Keine Netzdateien.
0.7.1 Autorenkorrektur: id author Dirk D. Sommerfeld; Windows, drei Android-ABIs und drei ESP32-Profile neu gebaut und unter den bisherigen 0.7.1-Dateinamen ersetzt. Manifest-Pruefsummen aktualisiert, UCI-Autor und alle sieben Runtime-Binaries kontrolliert; BearChess-0.7.1-Autorcache korrigiert.
2026-10-31: Android-Bibliotheks-Build hinzugefuegt: libvirgin.so fuer arm64-v8a (681008 B), armeabi-v7a (442852 B), x86_64 (681744 B), JNI/Java-Wrapper, C-API und AAR. Unveraenderte Suche/Bewertung, Autor Dirk D. Sommerfeld. UCI-Streams isoliert, Sitzungen seriell wiederverwendbar, stop/quit/EOF beenden Worker vor Freigabe. 116 Selbsttests, Host-Lebensdauertests und Android-14-x86_64-JNI-Test bestanden; ELF 16-KiB-Alignment aller ABIs verifiziert. Ausgabe dist/virgin-android-library-0.7.1-20261031. Bestehende Releases per SHA-256 unveraendert.
2026-10-31: 0.8.0 bereit: Gewinnchancenskalierung fuer RB/RN gegen R, R gegen Minor, NN gegen K; falscher Laeufer/Randbauern-Koenigsecke; Zugzwangschutz in sparsamen Endspielen; direkte Bauernsturmgefahr und verbleibende Dame-plus-Figur-Koenigsgefahr. 127 Selbsttests normal/kompakt, 680 TalChooser-Paritaetsfaelle, UCI/Timing/Adaptive/C-API/JNI bestaetigt. Partie-4-Endstellung bei 200000 Knoten: 0.7.1 +530 cp, 0.8.0 +63 cp. Andere Mittelspielschwaechen bleiben; kein Elo-/Matchnachweis. Windows/Android, Bibliothek/AAR und drei ESP32-Firmwares in neuen dist/0.8.0-Paketen, alte 0.7.1-Manifeste unveraendert. Hardware-ESP32/ARM nicht getestet.

2026-11-01: Core2 standalone GUI: manual BLE discovery/selection only, no reconnect; adaptive-only game controller, physical move/LED confirmation, live depth/white-perspective score/PV, crisp Merida pieces. Classical ESP32 16MB/PSRAM; 1MB TT, 72KiB internal search stack, MAX_PLY20. Host model/protocol tests: 22 passed. Physical board BLE connection remains unverified. Original Core2 flash backed up and verified before installation.

2026-11-01: Core2 display-fix2: PSRAM frame buffer and sidebar-only refresh remove visible redraw clears; engine move is previewed before physical execution and held during piece lifts. Logical move commits only on full physical match. Status labels Nachziehen/Du am Zug corrected. 24 host checks and MCU engine/screenshot tests passed.

2026-11-01: Stilnamen Fortress/Stone/Normal/Fire/Thunder; kanonische englische Stil-/Bot-IDs, deutsche Namen und alte Bot-IDs als Aliase. UCI/Kataloge/README aktualisiert. 95 Gewichte je Stil identisch mit 0.8.0; Windows, Android-UCI und JNI/AAR fuer drei ABIs separat unter dist/virgin-0.8.0-20261101-styles. Core2-Installation unveraendert.

2026-11-02: Core2 auto-start-move: full starting arrangement (either physical orientation) starts a fresh game, defaults human to White; edge-trigger prevents duplicate starting reports from cancelling the opening. New Game menu replaced by Move: hands side to move to Virgin and rotates view, or stops an existing search early. Pending move must be physically completed. Restart clears analysis/adaptive history and rejects stale search replies. 37 host checks passed, ESP32 build passed, flashed COM17; UART virtual-board MCU checks and final disconnected boot passed. Real-board handling of new controls awaits user play test. Package dist/virgin-core2-0.8.0-20261102-auto-start-move.

2026-11-02: Core2 queen: six original BeatVirgin logo queen moods at 42x62, no track/bar. Uses adaptive selected-move after score from player perspective; fallback negated engine cp/mate, exact BeatVirgin smiley thresholds. Updates on accepted engine result, inverts on Move side switch, resets on new game/draw, overrides on checkmate. 50 host checks, firmware build/COM17 flash, MCU engine/Move/restart/queen diagnostics and display capture passed. Retains manual BLE and buffered drawing. Real-board play test of new UI remains with user. Package dist/virgin-core2-0.8.0-20261102-queen.

2026-11-02: Core2 Certabo/Spectrum BLE NUS integrated from local gateway: fresh 34-ID calibration per manual connection, early d3/d6 mono vs e3/e6 RGB queen selection, three confirmation flashes, spare queen removal gate, correct canonical positions and LEDs. Queue sequence gap detection, MTU256 request, no-response LED writes, no speculative LED format, no calibration timeout/reconnect. Spectrum play colours follow selected-move player eval using chesstactics five-stop scale and the hardware-specific spectrumTint blue correction; setup blue80. 50 existing Core2 checks, 565 new Certabo checks and five original gateway regression suites passed. Firmware built/flashed COM17; MCU engine/Move/queen/restart regression and final disconnected boot passed. Certabo/Spectrum actual BLE and physical LEDs remain untested on Core2. Package dist/virgin-core2-0.8.0-20261102-certabo-spectrum.

2026-11-02: Core2 battery header: horizontal battery at far right with percent and charging bolt, red at <=20%; cached M5.Power battery/charging readings every 5s, unknown level shown as --%. Header region refreshed separately even during sidebar-only draws. Existing Adaptive, queen, Move, restart and BLE game/protocol logic unchanged. Firmware built successfully. Flash and display QA pending: Core2 USB port absent. Package dist/virgin-core2-0.8.0-20261102-battery. Certabo/Spectrum physical board test remains pending.

2026-11-02: Core2 battery header: horizontal battery at far right with percent and charging bolt, red at <=20%; cached M5.Power battery/charging readings every 5s, unknown level shown as --%. Header region refreshed separately even during sidebar-only draws. Existing Adaptive, queen, Move, restart and BLE game/protocol logic unchanged. Firmware built and flashed COM17, actual PMIC level/charging status and LCD screenshot checked via read-only UART; no game/board commands or reset during UI QA. Package dist/virgin-core2-0.8.0-20261102-battery. Certabo/Spectrum physical board test remains pending.
