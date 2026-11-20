# Partieleistung aus Virgin 0.8.5 abrufen

An die Engine senden:

```text
performance
```

Antwort zum Beispiel:

```text
info string performance white 2300 black >2800 window 3 active true white_samples 5 black_samples 4 white_ply 9 black_ply 8 pending 0 revision 12 skipped 0 white_lossguard false black_lossguard false status ready
```

Die App liest den Wert hinter `white` oder `black` fuer die Spielerfarbe. `>2800` ist Text. Weitere Felder darf sie ignorieren; bei `pending` groesser als 0 behaelt sie vorerst den bisherigen Wert.

In Adaptive ist die Bewertung automatisch aktiv. Sonst einmal einschalten:

```text
setoption name LivePerformance value true
```

Die App fragt nach der normalen Engine-Antwort `bestmove` ab. `position ... moves ...` und `go ...` bleiben wie bisher; die Abfrage selbst startet oder stoppt keine Suche. Bei einer neuen Partie setzt `ucinewgame` die Werte zurueck. Ohne Abfrage muss eine GUI nichts anzeigen.
