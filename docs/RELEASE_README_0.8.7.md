# Virgin 0.8.7 Beta - 20. November 2026

Globales Eröffnungsbuch (90 Varianten, 2.037 Byte Buchdaten, `OwnBook=true`)
und adaptive Zufallsauswahl sicherer gleichwertiger Züge mit Taktikschutz.

| Ordner | Ausgabe |
|---|---|
| windows | Windows x64: UCI-EXE, DLL, Compact-Host, C-API-Header und Daten |
| android | Signierte OEX-APK, AAR, JNI/UCI für arm64-v8a, armeabi-v7a, x86_64 und x86 |
| web | WebAssembly, Worker, Demo, lokaler Server und separates Web-Quellarchiv |
| linux | Statische UCI-Engines: x86_64, ARMv6 Hardfloat, ARMv7 Hardfloat |
| esp32-s3 | mini_no_psram, mini_psram_qspi, mini_psram_opi |
| esp32-core2 | M5Stack Core2: Bootloader, Partitionen und Firmware |
| source | Quellstand und Build-Werkzeuge ohne private Signierschlüssel |
| checks | Prüfberichte, Quellprüfsummen und Build-Inventar |

`setoption name OwnBook value false` schaltet das Buch global aus.
Die adaptive Zufallsauswahl ist davon unabhängig. Ausführliche Informationen:
`RELEASE_0.8.7.md` und `OPENING_BOOK.md`.

Die eingestellte Spielweise übernimmt außerhalb des Buchs. Für Analyse mit
fester Tiefe das Buch ausschalten; `go infinite` rechnet stets ohne Buch.

Nichts wurde auf Geräte geflasht oder installiert. Laufzeit- und Strukturprüfungen
sind im Manifest getrennt ausgewiesen. Die Performance-Anzeige ist keine
kalibrierte Turnier-Elo. `SHA256SUMS.txt` enthält die Prüfsummen des Release-Pakets.
