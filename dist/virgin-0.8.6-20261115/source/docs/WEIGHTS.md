# Gewichte: alles von außen steuerbar

Jeder Parameter der Engine steht in **einer** Registry
(`src/weights.cpp`, Tabelle `WEIGHT_DEFS`) und wird daraus automatisch

1. als **UCI-Option** angemeldet (`option name <Name> type spin default ... min ... max`),
2. in die Datei `data/weights_default.txt` geschrieben (`virgin --gen-data`),
3. beim Start und bei `ReloadWeights` wieder eingelesen.

Es gibt also keine versteckten Konstanten: was die Engine bewertet, kann von
außen verändert werden - per GUI-Slider, per Textdatei oder aus der App.

## Drei Wege, ein Gewicht zu ändern

```text
# 1) Laufzeit (GUI-Slider oder App)
setoption name Aggression.KingAttack value 220
setoption name Group.KingSafety value 70

# 2) Datei: data/weights_default.txt bearbeiten, dann
setoption name ReloadWeights

# 3) Eigene Datei
setoption name WeightsFile value C:\meine-gewichte.txt
```

Alle Werte werden auf `min..max` begrenzt (Schutz vor kaputten
Konfigurationen). `setoption name ResetWeights` stellt die Standardwerte her,
`setoption name ShowWeights` gibt alle aktuellen Werte aus.

## Parameterübersicht (91 Parameter)

Termwerte in Centipawns, Gruppen-Multiplikatoren in Prozent (100 = neutral).

### Material (mg = Mittelspiel, eg = Endspiel)

Kalibriert nach dem Konsens von Arasan 23.4, PeSTO und den üblichen
handgeschriebenen Engines (siehe `docs/NOTES.md`).

| Name | Standard | Bereich |
|---|---|---|
| Material.Pawn.Mg / .Eg | 100 / 107 | 0..1600 |
| Material.Knight.Mg / .Eg | 395 / 377 | 0..1600 |
| Material.Bishop.Mg / .Eg | 405 / 400 | 0..1600 |
| Material.Rook.Mg / .Eg | 595 / 620 | 0..1600 |
| Material.Queen.Mg / .Eg | 1225 / 1180 | 0..2000 |

### Figurenfeld-Tabellen (Deltas auf die PeSTO-Basis)

Die eigentliche PST kommt seit dem PeSTO-Umbau aus `src/pst_pesto.h`
(veröffentlichte, getunte Tabellen - siehe `docs/NOTES.md`). Die folgenden
12 Gewichte sind **Abweichungen** davon und werden additiv aufgesetzt:

`Pst.PawnMgAdvance` 8, `Pst.PawnEgAdvance` 14, `Pst.KnightCenter` 10,
`Pst.KnightRim` -12, `Pst.BishopCenter` 6, `Pst.BishopDiagonal` 2,
`Pst.RookRank7` 0, `Pst.RookFileCenter` 4, `Pst.QueenCenter` 3,
`Pst.KingMgShelter` 22, `Pst.KingMgCenter` -12, `Pst.KingEgCenter` 12

`Pst.RookRank7` steht bewusst auf 0: die PeSTO-Tabelle enthält den Bonus für die
7. Reihe schon (+62..+80), Arasan bewertet ihn zusätzlich mit 0 (mg) / 9 (eg).
`Rooks.OnSeventh` ist deshalb von 15 auf 8 gesenkt.

Wichtig: Ein Wert von 0 bedeutet hier "genau wie PeSTO", nicht "kein PST".


### Mobilität

Die Gewichte zählen **Züge über oder unter vier sicheren Zügen** (vier = neutral).
Das ist die Form aus Arasans getunten Tabellen (`KNIGHT_MOBILITY`,
`BISHOP_MOBILITY`, ... in `ref/arasan-v23.4.0/params.cpp`): dort ist ein Springer
ohne Züge ≈ −66 cp wert und ein sehr beweglicher nur ≈ +12, also keine Gerade
durch den Nullpunkt. Vorher war `Züge × Gewicht` gerechnet, was bewegliche
Figuren stark über- und unbewegliche gar nicht bewertete.

`Mobility.Knight` 4, `Mobility.Bishop` 5, `Mobility.Rook` 2, `Mobility.Queen` 1

Zusätzlich bekommt eine **Leichtfigur mit höchstens einem sicheren Zug** eine
Strafe (viermal ihr Mobilitätsgewicht) - das ist Arasans "eingesperrter Läufer"
(bei ihm rund −150 cp), nur ohne neues Gewicht ausgedrückt.

### Königssicherheit
`KingSafety.AttackUnits` 100, `KingSafety.Saturation` 4096,
`KingSafety.ShieldMissing` -22, `KingSafety.ShieldPawn` 8,
`KingSafety.OpenFileNearKing` -28, `KingSafety.SemiOpenNearKing` -14,
`KingSafety.TropismN` 12, `KingSafety.TropismB` 10, `KingSafety.TropismR` 8,
`KingSafety.TropismQ` 6, `KingSafety.SafeCheck` 18,
`KingSafety.UncastledMg` -30, `KingSafety.KingRingAttack` 6

### Bauernstruktur

`Pawns.PassedMg` und `Pawns.PassedEg` bedeuten **den Bonus für einen Freibauern
auf der 7. Reihe** (einen Schritt vor der Umwandlung). Die Staffelung nach Reihe
kommt aus Arasans getunter Tabelle und steigt steil an (5. Reihe 33 %, 6. Reihe
100 % im Mittelspiel; im Endspiel 23 %, 55 %, 100 %) - vorher war es eine
Gerade. Werte: 100 / 100.

`Pawns.Doubled` -12, `Pawns.Isolated` -15, `Pawns.Backward` -8,
`Pawns.Connected` 4, `Pawns.Candidate` 8, `Pawns.DoubledVsKing` -18

`Pawns.Doubled` -12, `Pawns.Isolated` -15, `Pawns.Backward` -8,
`Pawns.Connected` 6, `Pawns.PassedMg` 20, `Pawns.PassedEg` 40,
`Pawns.Candidate` 8, `Pawns.DoubledVsKing` -18

### Türme / offene Linien
`Rooks.OpenFile` 25, `Rooks.SemiOpenFile` 12, `Rooks.OnSeventh` 15,
`Rooks.Connected` 10, `Rooks.Trapped` -40, `Rooks.OpenFileVsKing` 30

### Figuren und Bedrohungen
`Pieces.KnightOutpost` 25, `Pieces.BishopPair` 30, `Pieces.BadBishop` -3,
`Pieces.Fianchetto` 12, `Threats.AttackedByLower` 20, `Threats.Hanging` 45,
`Threats.Pinned` 15, `Threats.Discovered` 18

### Raum, Tempo, Initiative
`Space.PerSafeSquare` 8, `Initiative.Tempo` 10, `Initiative.Progress` 12

### Aggression (der Stilantrieb)
| Name | Standard | Wirkung |
|---|---|---|
| Aggression.KingAttack | 100 | Angriffseinheiten gegen den feindlichen König |
| Aggression.PawnStorm | 100 | Bauernvorstöße auf den feindlichen König |
| Aggression.Initiative | 100 | Fortschritts-/Tempobonus für Angriffszüge |
| Aggression.SacrificeTolerance | 100 | wie viel Material für Angriff geopfert werden darf |
| Aggression.AvoidQueenTrade | 100 | Strafe für frühen Damentausch |

### Komplexität, Remis, Endspiel, Gruppen, Sonstiges
`Complexity.Weight` 100 (Schärfe/Kompliziertheit der Stellung),
`Drawish.OppositeBishops` 60, `Drawish.Insufficient` 0,
`Endgame.KingActivity` 100, `Endgame.PassedKingProximity` 100,
`Group.Material/Pst/Mobility/KingSafety/Pawns/Rooks/Pieces/Threats/Space/Initiative/Aggression/Complexity/Drawish/Endgame` je 100,
`Misc.Contempt` 0 (-100..100), `Misc.MoveNoise` 0 (0..400).

## Was jetzt wirkt

Seit Meilenstein 5 sind **alle** Terme verdrahtet. Aktiv sind:

* **Material** und **PST** (12 Formparameter, inkl. `Endgame.KingActivity` als
  Multiplikator auf den Endspiel-Königsteil)
* **Mobilität** je Figur (Felder, die von feindlichen Bauern gedeckt sind,
  zählen nicht)
* **Königssicherheit**: Angriffseinheiten (quadratisch mit Sättigung),
  Bauernschild, offene/halboffene Linie am König, Angreifer im Königsring,
  sichere Angreifer, unrochiert-Strafe
* **Bauernstruktur**: Doppel-, Isolier-, Rückstands-, Verbund-, Frei- und
  Kandidatenbauern, Doppelbauern vor dem eigenen König
* **Türme**: offene/halboffene Linie, 7. Reihe, verbundene Türme, offene Linie
  gegen den König, eingesperrter Turm
* **Figuren**: Läuferpaar, schlechter Läufer, Springer-Außenposten, Fianchetto
* **Bedrohungen**: angegriffen von geringerwertiger Figur, hängend, gefesselt,
  aufgedeckter Angriff
* **Raum**, **Initiative** (Tempo + Fortschritt), **Endspiel** (Königsnähe zu
  Freibauern), **Remis-Skalierung** (ungleichfarbige Läufer, unzureichendes
  Material)
* **Aggression** (König-Angriff, Bauernsturm, Initiative, Opfer-Toleranz,
  Damentausch-Vermeidung) und **Komplexität/Schärfe** - die Stilantriebe
* **`Misc.Contempt`**: ein Remis wird im Suchbaum mit `-Contempt` bewertet,
  ein aggressiver Bot meidet also Wiederholungen
* **`Elo.KnowledgeFloor`**: Mindestwissen der schwachen Bots (Standard 25 %)
* alle 14 **Gruppen-Multiplikatoren**

Die Bewertung ist in 13 Terme aufgeteilt und über `eval` abrufbar:

```text
info string eval total 53 cp (material 0, pst 8, tempo 10, phase 24)
info string terms: material=0 pst=8 mobility=9 kingSafety=0 pawns=14
  rooks=0 pieces=0 threats=0 space=0 initiative=0 endgame=0 aggression=12 complexity=0
```

Die Startwerte sind geschätzt; Texel-Tuning (offen) ersetzt sie durch
kalibrierte Zahlen.
