# Bots: 215 Profile aus 43 Stärkestufen x 5 Stilen

Ein **Bot** ist die Kombination aus Stärke (Elo) und Spielstil. Der Katalog
wird von der Engine erzeugt (`virgin --gen-data`) und liegt in zwei Formaten:

| Datei | Für wen | Inhalt |
|---|---|---|
| `data/bots.txt` | die Engine | `id elo style "Anzeigename" tags` |
| `data/bots.json` | die App | vollständiger Katalog inkl. Stilgewichte und Elo-Profil |

Erzeugt werden beide von der Engine selbst: `virgin --gen-data` (Quelle:
`src/botdata.cpp`). Es ist keine Hilfssprache beteiligt.

Aktuell: **43 Stufen** (700 bis 2800 in 50er-Schritten) x **5 Stile** = **215 Bots**.
Wenn nur ~120 gebraucht werden: im 100er-Raster sind es 22 x 5 = 110 Bots
(Filter in der App, z. B. `elo % 100 == 0`).

## Die fünf Stile (Achse "Mut")

| Level | id | Name | Charakter |
|---|---|---|---|
| 1 | `furchtsam` | Furchtsam | Sicherheit zuerst, meidet Schärfe, nimmt Remis an |
| 2 | `vorsichtig` | Vorsichtig | solide, positionell, geringes Risiko |
| 3 | `ausgeglichen` | Ausgeglichen | Referenzstil (alle Gewichte Standard) |
| 4 | `angriffslustig` | Angriffslustig | sucht den Königsangriff, meidet Damentausch |
| 5 | `stier` | Stier | tollkühn: Opfer, Bauernsturm, König jagt König |

Was den Stil technisch ausmacht (Werte in `data/styles.txt`, alle einzeln
änderbar): `Aggression.KingAttack`, `Aggression.PawnStorm`,
`Aggression.Initiative`, `Aggression.SacrificeTolerance`,
`Aggression.AvoidQueenTrade`, `Complexity.Weight`, `Misc.Contempt` sowie die
Gruppen-Multiplikatoren für Königssicherheit, Bauern, Material und Remisneigung.

Zusätzlich wirkt der Stil auf die **Suche**: `Misc.MoveNoise` legt fest, wie
viele Centipawns ein Zug vom besten entfernt sein darf, um trotzdem gewählt zu
werden. Das macht die Bots unterscheidbar und menschlicher (nicht nur
"schwächer").

## Beispiel: Stil 1 gegen Stil 5 (Werte)

| Gewicht | Furchtsam | Stier |
|---|---|---|
| Misc.Contempt | -25 | 45 |
| Group.KingSafety | 200 | 70 |
| Aggression.KingAttack | 40 | 220 |
| Aggression.PawnStorm | 0 | 250 |
| Aggression.SacrificeTolerance | 0 | 300 |
| Aggression.AvoidQueenTrade | 0 | 200 |
| Complexity.Weight | 30 | 220 |
| Rooks.OpenFileVsKing | 15 | 54 |

## Stärkestufen (`data/elo_profile.txt`)

Pro Stufe stehen dort `MaxDepth`, `NodeLimit`, `MoveError`, `BlunderPct`,
`NpsTarget`, `MoveNoise` und für jeden Bewertungsterm `Knowledge.<Term>` =
`start:full` (Elo, ab der der Term anfängt bzw. vollständig wirkt).

| Elo | MaxDepth | NodeLimit | MoveError | BlunderPct |
|---|---|---|---|---|
| 700 | 2 | 5.000 | 90 | 11,0 |
| 1000 | 4 | 27.000 | 67 | 8,1 |
| 1500 | 7 | 258.000 | 42 | 5,9 |
| 2000 | 10 | 2.462.000 | 21 | 4,0 |
| 2400 | 13 | 8.890.000 | 12 | 3,2 |
| 2800 | 22 | 20.000.000 | 8 | 2,8 |

(Werte sind Startwerte und werden in Meilenstein 5 gegen Referenzengines
kalibriert.)

## So startet die App einen Bot

```text
# Variante A: ein Befehl (empfohlen)
setoption name Bot value v1200-stier

# Variante B: getrennt
setoption name UCI_LimitStrength value true
setoption name UCI_Elo value 1200
setoption name Style value Stier

# Variante C: Zwischenstufe (10 = furchtsam ... 50 = stier, dazwischen interpoliert)
setoption name Boldness value 45

# Katalog abfragen
listbots
-> info string bot v700-furchtsam 700 furchtsam "Maus 700" defensive,solid,drawish
-> ...
-> info string bots 215

# Ohne UCI-Protokoll: Prozess mit Argument starten
virgin --bot v1200-stier
virgin --list-bots
```

Empfohlener Ablauf in der App beim Anlegen eines Bots:

1. `data/bots.json` einlesen (Stile, Elo-Profil, Bot-Liste).
2. Nutzer wählt Stufe und Stil (oder Regler 1..5).
3. Engine starten, `setoption name Bot value <id>` senden.
4. Optional Feintuning: einzelne Gewichte per `setoption` überschreiben,
   z. B. `setoption name Aggression.KingAttack value 260`.
5. Gespeicherte Bot-Definitionen kann die App als eigene Gewichtsdatei
   schreiben und per `setoption name WeightsFile value <pfad>` + `ReloadWeights`
   laden - dann sind die Bots vollständig datengetrieben.

## Stand

Katalog, Optionen und Anwendung sind fertig: `setoption name Style value <Name>`
setzt die Stilgewichte sofort, `setoption name Bot value v1200-stier` zusätzlich
Elo-Wert und `Misc.MoveNoise`. Sichtbar wird das z. B. mit
`virgin --selfplay v1200-furchtsam v1200-stier 60 5`.

Noch offen (M4): Suchtiefen-/Node-Begrenzung und Blunder-Modell je Elo-Stufe,
`Boldness`-Interpolation, Eröffnungsbuch. Siehe `docs/ROADMAP.md`.
