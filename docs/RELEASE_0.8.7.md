# Virgin 0.8.7 Beta

Dirk D. Sommerfeld, 20. November 2026. Grundlage: Virgin 0.8.6.

## Eröffnungsvielfalt

- Globales eingebettetes Eröffnungsbuch für alle Spielmodi, Elo-Stufen und Stile.
- UCI-Option `OwnBook`, standardmäßig `true`; auch im adaptiven Modus schaltbar.
- 90 kuratierte Varianten mit jeweils zehn Halbzügen. Buchdaten: 2.037 Byte
  einschließlich Startstellung; beim Kompilieren auf weniger als 10.000 Byte begrenzt.
- Gleichverteilte Auswahl unterschiedlicher Buchfortsetzungen, unabhängig von
  Suchbewertung und Anzahl der dahinterliegenden Varianten.
- Nach Buchende oder Abweichung übernimmt die jeweilige Suche. Analyse und
  explizite `searchmoves` rechnen ohne Buch. Für feste Tiefenanalyse `OwnBook=false`.
- Keine externen Buchdateien nötig. Details: `OPENING_BOOK.md`.

## Adaptiver Modus

Sichere Kandidaten innerhalb von 20 Centipawns des adaptiven Zielzugs werden
immer zufällig ausgewählt, auch ohne Buch und bei fester Suchtiefe. Die bisherige
15-Prozent-Hürde und der zusätzliche Vergleich mit der vorherigen Suchtiefe
entfallen. Erkannte Materialtaktiken, laufende Taktikfolgen, Mattvarianten und
Schachgebote gegen den eigenen König haben Vorrang. Der Schutz beruht auf dem
berechneten Horizont; er garantiert keine Fehlerfreiheit außerhalb der Suche.

## Ausgaben

Windows x64 (UCI, DLL, Compact-Host), Android (vier Architekturen als UCI/JNI,
AAR und signierte universelle OEX-APK), Browser/WebAssembly, Linux x86_64/ARMv6/
ARMv7 sowie ESP32-S3 in drei Konfigurationen und M5Stack Core2.

Firmware wurde nur gebaut, nicht auf Geräte geflasht. Android-, Linux-ARM- und
ESP32-Geräte sind nicht durch einen erfolgreichen Cross-Build laufzeitgeprüft.
Die tatsächlichen Prüfberichte liegen im Release-Ordner `checks`.
