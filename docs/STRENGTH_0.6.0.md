# Virgin 0.6.0 – Figurenfallen und Freibauern

31.10.2026. Separater Kandidat nach Virgin 0.5.0. Ziel bleibt Carballo bei 1 s/Zug klar schlagen; das ist noch nicht nachgewiesen. Die neue Remispartie liegt bisher nur mit PGN-Kopf vor und wurde daher nicht analysiert. Keine Spiele gegen andere Engines gestartet.

## Ausgewählte Änderungen

Die Beweglichkeit jeder Leichtfigur wird einzeln bewertet. Ein aktiver Läufer kann einen eingesperrten zweiten Läufer nicht mehr über die summierte Mobilität verdecken. Für eine von einem Bauern angegriffene Leichtfigur berücksichtigt der zusätzliche Engpass-Malus auch Fluchtfelder unter gegnerischer Springer-, Läufer- und Königskontrolle. Schläge gegen Türme oder Damen bleiben mögliche materielle Auswege. Das ist eine begrenzte statische Näherung, kein vollständiger Beweis einer verlorenen Figur.

Freibauern erhalten ihren bisherigen rangabhängigen Bonus abhängig vom Vormarsch: direkt durch eine gegnerische Figur blockiert 50 %, durch eigene Figur blockiert 75 %, unzureichend gesichertes nächstes Feld 75 %, vollständig freie und nicht angegriffene Laufbahn 125 %, sonst 100 %. Ein hinter einem eigenen Bauern stehender Doppelbauer zählt nicht zusätzlich als Freibauer. Die Angriffskarten berücksichtigen wie zuvor absolute Fesselungen.

Der zusätzliche Test einer Nullzug-Verifikation wurde wegen seines deutlichen Tiefenverlusts verworfen. Die bewährte Suchselektivität aus 0.5.0 bleibt im ausgelieferten Kandidaten erhalten. Verwarfene Suchvarianten liegen unter `experiments/search-0.6-20261031`. Nebenbei stimmt die ausgegebene Hauptvariante nach der absichtlichen Zugauswahl schwächerer Persönlichkeiten wieder mit deren gewähltem Zug überein.

Keine neuen Gewichtsparameter, keine Änderung am Persönlichkeitskatalog und kein neuronales Netz. Alle Bewertungsänderungen wirken innerhalb der vorhandenen Mobilitäts- und Bauerngewichte und respektieren die Gruppensteuerung der Persönlichkeiten.

## Prüfung und Aussagegrenzen

- 105 interne Windows-Prüfungen bestanden. Vier neue Prüfungen betreffen den verdeckten gefangenen Läufer, durch einen Springer versperrte Fluchtfelder, blockierte Freibauern und doppelt gezählte Freibauern.
- Unabhängiges Schlagfolgen-Orakel weiterhin 319 Fälle, 0 Unterschiede.
- UCI-Prozesstest mit einem/vier Threads, Optionswechsel während der Suche, 100-ms-Grenze und regulärem Beenden bestanden.
- 24 Positionsproben bei 1 s/Zug, ein Thread, 32 MiB Hash, Spielstärkebegrenzung aus, beide Stile. Weitere zwölf Proben bei 200.000 Knoten. Gemeldete Hauptvarianten unabhängig mit python-chess legal nachgespielt. Die Probeprotokolle sind beigefügt.
- Android für ARM64, ARM32 und x86_64 mit NDK 27/API 21 gebaut; ELF-Architektur und PIE geprüft. Nur libc/libm/libdl benötigt. Mangels angeschlossenem Gerät keine Ausführung oder Installation auf Android geprüft.

Die nachfolgenden Messungen sind einzelne Windows-Läufe und kein Elo-Test. Im Angriffsstil wird vor 16...Sd7 aus Verlustpartie 2 jetzt ...Sc4 gewählt. Die bereits nach 17.f5 entstandene Läuferfalle bleibt eine schlechte Stellung; eine gelöste Partie wird nicht behauptet. Andere kritische Züge bleiben teilweise bestehen. Median der Tiefendifferenz gegenüber dem gespeicherten 0.5-Lauf: +0; unterschiedliche Varianten und Bewertungen schränken die Aussagekraft von Tiefenvergleichen ein.

| Stellung (gespielte Halbzüge) | Stil | Zug 0.5 | Zug 0.6 | Tiefe 0.5 | Tiefe 0.6 |
|---|---|---|---|---:|---:|
| loss1-ply24 | Ausgeglichen | g4 | g4 | 12 | 12 |
| loss1-ply28 | Ausgeglichen | f4 | f4 | 12 | 11 |
| loss1-ply34 | Ausgeglichen | Kg2 | Rd3 | 10 | 10 |
| loss1-ply62 | Ausgeglichen | Rxc4 | Rxc4 | 18 | 18 |
| loss1-ply94 | Ausgeglichen | Ke3 | Ke3 | 20 | 20 |
| loss1-ply98 | Ausgeglichen | Kc3 | Kc3 | 23 | 23 |
| loss2-ply7 | Ausgeglichen | b5 | b5 | 11 | 11 |
| loss2-ply31 | Ausgeglichen | Nc4 | Nc4 | 13 | 13 |
| loss2-ply33 | Ausgeglichen | Qc5 | c6 | 13 | 13 |
| loss2-ply43 | Ausgeglichen | Qxg6 | Qxg6 | 14 | 14 |
| loss2-ply99 | Ausgeglichen | Rxc3 | Rxc3 | 14 | 15 |
| loss2-ply105 | Ausgeglichen | cxb4 | cxb4 | 17 | 17 |
| loss1-ply24 | Angriffslustig | f4 | Nd2 | 12 | 13 |
| loss1-ply28 | Angriffslustig | Rd4 | f4 | 12 | 12 |
| loss1-ply34 | Angriffslustig | Rd3 | Rd3 | 12 | 10 |
| loss1-ply62 | Angriffslustig | Rxc4 | Rxc4 | 17 | 15 |
| loss1-ply94 | Angriffslustig | Ke3 | Ke3 | 20 | 20 |
| loss1-ply98 | Angriffslustig | Kc3 | Kc3 | 23 | 24 |
| loss2-ply7 | Angriffslustig | b5 | b5 | 11 | 11 |
| loss2-ply31 | Angriffslustig | Ned7 | Nc4 | 12 | 13 |
| loss2-ply33 | Angriffslustig | Qd6 | Qc5 | 12 | 12 |
| loss2-ply43 | Angriffslustig | Qxg6 | Qxg6 | 15 | 14 |
| loss2-ply99 | Angriffslustig | Rxc3 | Rxc3 | 15 | 15 |
| loss2-ply105 | Angriffslustig | cxb4 | cxb4 | 17 | 17 |

## Dateien

- `virgin-0.6.0.exe`: 419840 Bytes (410.0 KiB).
- `virgin-arm64-v8a`: 814872 Bytes (795.8 KiB).
- `virgin-armeabi-v7a`: 546800 Bytes (534.0 KiB).
- `virgin-x86_64`: 846152 Bytes (826.3 KiB).

Windows-Datei als UCI-Engine in die selbst gewählte Oberfläche einbinden. Android verwendet normalerweise `virgin-arm64-v8a`, ältere 32-Bit-Geräte `virgin-armeabi-v7a`, x86-Geräte/Emulatoren `virgin-x86_64`. Dies sind UCI-Binärdateien, keine APKs. Der Nutzer wählt Oberfläche und Import selbst. Kein Gerät verändert.

Für Vergleichspartien `UCI_LimitStrength=false`, 1 s/Zug und identische Stil-/Thread-/Hash-Einstellungen verwenden. Frühere Versionen bleiben erhalten, insbesondere `dist/meine-engine`. SHA256 und Dateigrößen stehen in `manifest.json`. Sicherung des vorherigen Quellstands: `backups/before-0.6-20261031-142152`.
