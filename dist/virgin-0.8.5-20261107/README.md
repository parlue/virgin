# Virgin 0.8.5 Beta – 7. November 2026

Alle Dateien liegen direkt in den Plattformordnern; keine Installation und kein Flashen durchgeführt.

| Ordner | Dateien / Ziel |
|---|---|
| windows | Windows x64: UCI-EXE, DLL, C-API-Header, Daten |
| android | AAR, JNI und UCI für arm64-v8a, armeabi-v7a, x86_64, x86; Einbindungsanleitung |
| web | WebAssembly, JavaScript-Worker, Demo und lokaler Server |
| esp32-s3 | mini_no_psram, mini_psram_qspi, mini_psram_opi: je Bootloader, Partitionen, Firmware |
| linux | Statische UCI-Engines für x86_64, ARMv6 Hardfloat und ARMv7 Hardfloat; Daten |
| source | Quellstand und Build-Dateien dieser Version, ohne private Signierschlüssel |
| checks | Prüfberichte und Build-Inventar |

Anzeige abrufen: `performance`. In Adaptive automatisch aktiv, sonst
`setoption name LivePerformance value true`. Details: `APP_PERFORMANCE_0.8.5.md`.

Windows, DLL, kompakter Host-Kern, WebAssembly und Linux x64 wurden ausgeführt
und geprüft. Android-Binärdateien und ARM-Linux-Dateien wurden strukturell geprüft;
keine eigenen Laufzeittests auf Android-/ARM-/ESP32-Geräten. Core2 bleibt bei seinem
bisherigen separaten Stand und ist kein Ziel dieses 0.8.5-Updates.

Die Anzeige beschreibt die laufende Partie; sie ist keine kalibrierte Turnier-Elo.
SHA256SUMS.txt erlaubt die Prüfung aller ausgelieferten Dateien.
