# Virgin 0.8.6 Beta

Dirk D. Sommerfeld, 15. November 2026. Grundlage: Virgin 0.8.5.

## Suchstärke

Nur die Suche wurde geändert, die Bewertung (Eval) und alle UCI-Optionen sind
unverändert. Ausgangspunkt war der Eindruck zu geringer Suchtiefe; besonders
Endspiele waren flach (Turmendspiel mit Bauern: Tiefe 8 in 1 s).

- **Endspiele:** Futility, Late Move Pruning und Late Move Reductions waren in
  jeder dünnen Stellung abgeschaltet. Jetzt nur noch in reinen Bauernendspielen.
  Null-Move, Razoring und Reverse Futility bleiben vorsichtig wie bisher.
  Turmendspiel: Tiefe 12 statt 8 in 1 s.
- **SEE-Pruning:** Schlagzüge und ruhige Züge, die zu viel Material verlieren,
  werden bei geringer Resttiefe (höchstens 8 Halbzüge) nicht mehr durchsucht.
- **Capture History:** lernt, welche Schlagzüge Abschneidungen auslösen, und
  ordnet Schlagzüge damit besser (ca. 28 KB zusätzlicher Speicher je Thread).
- **Reduktionen:** Late Move Reductions etwas aggressiver (Divisor 2,36 -> 1,75).

## Messung (Selbstvergleich gegen 0.8.5, 1 Thread, gleiche Zeit je Seite)

| Schritt | Partien | Zeit/Zug | Ergebnis |
|---|---|---|---|
| Endspiel-Pruning | 200 / 150 | 100 / 300 ms | +80 +/- 47 / +115 +/- 55 Elo |
| + SEE-Pruning | 200 | 150 ms | +65 +/- 47 Elo |
| + Capture History | 200 | 150 ms | +45 +/- 46 Elo |
| Zwischenstand ohne LMR vs. 0.8.5 | 200 | 200 ms | +104 +/- 47 Elo |
| LMR-Divisor 1,45 vs. ohne LMR-Änderung | 120 | 400 ms | +101 +/- 61 Elo |

Die Einzelwerte bei kurzer Bedenkzeit summieren sich nicht: Bei 400 ms war der
LMR-Gewinn deutlich kleiner als im Blitz. Der Gesamtgewinn von 0.8.6 gegenüber
0.8.5 ist nicht als eine Zahl gemessen. Gegen andere Engines, bei langer
Bedenkzeit und auf Android, ARM und ESP32 wurde die Stärke nicht gemessen.

## Prüfung

Selbsttest (Windows und kompakter Kern), UCI-Prüfung, Live-Performance-Prüfung
(14 Prüfungen), Linux x86_64 (ausgeführt in WSL, Selbsttest und UCI), alle drei
Linux-ELF-Dateien strukturell, WebAssembly im Chrome-Browser. Alle bestanden.

Nicht auf Geräten ausgeführt: Android, Linux ARMv6/ARMv7, ESP32-S3
(Firmware gebaut, nicht geflasht). Core2 bleibt bei seinem eigenen Stand und ist
kein Ziel dieses Updates.
