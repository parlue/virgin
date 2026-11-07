# Android-Build und Installation

## Voraussetzungen (auf diesem Rechner vorhanden)

* Android NDK **27.0.12077973** unter
  `%LOCALAPPDATA%\Android\Sdk\ndk\27.0.12077973`
* CMake 3.31.6, Ninja (aus den VS-2022-BuildTools)
* Kein Android Studio, kein Gradle, kein Java nötig

## Bauen

```powershell
cd C:\Users\DirkSommerfeld\Documents\Projects\virgin
.\build_android.ps1                      # arm64-v8a (Standard)
.\build_android.ps1 -Abi armeabi-v7a     # ältere Geräte
.\build_android.ps1 -Abi x86_64          # Emulator
.\build_android.ps1 -Abi all             # alle drei
```

Ergebnis: `build\android-<abi>\virgin` (bereits gestrippt).

Wichtige Einstellungen, die das Skript setzt:

| Einstellung | Wert | Grund |
|---|---|---|
| `ANDROID_ABI` | arm64-v8a / armeabi-v7a / x86_64 | Zielarchitektur |
| `ANDROID_PLATFORM` | android-21 | Mindest-API des NDK 27 |
| `ANDROID_STL` | c++_static | keine `libc++_shared.so` nötig |
| `VIRGIN_STATIC` | AUS (an mit `-Static`) | steuert vollständig statisches Linken |
| `-ffunction-sections -fdata-sections -Wl,--gc-sections` | an | kleine Binärdatei |
| `llvm-strip --strip-all` | an | Symboltabelle entfernen |

## Zwei Link-Modi

| Modus | Aufruf | Ergebnis | Wann benutzen |
|---|---|---|---|
| **PIE (Standard)** | `.\build_android.ps1` | ELF `DYN`, position independent, benötigt nur `libc.so`/`libm.so`/`libdl.so` (immer vorhanden) | Standard, maximale GUI-Kompatibilität |
| **statisch** | `.\build_android.ps1 -Static` | ELF `EXEC`, keine Abhängigkeiten (`NEEDED` leer, kein `INTERP`-Segment) | Wenn garantiert nichts dynamisch geladen werden darf |

Das Skript prüft das Ergebnis selbst mit `llvm-readelf` und gibt ELF-Typ und
benötigte Bibliotheken aus. Gemessene Werte (gestrippt):

| ABI | Modus | Größe | ELF-Typ | Abhängigkeiten |
|---|---|---|---|---|
| arm64-v8a | PIE (Standard) | 542 KB | DYN | `libm.so`, `libdl.so`, `libc.so` |
| arm64-v8a | `-Static` | 780 KB | EXEC | keine (`NEEDED` leer, kein `INTERP`) |
| armeabi-v7a | PIE (Standard) | 331 KB | DYN | `libm.so`, `libdl.so`, `libc.so` |
| armeabi-v7a | `-Static` | 480 KB | EXEC | keine |

Zum Vergleich: Stockfish-Android-Binaries mit NNUE sind 5-40 MB.

Hintergrund: Ab Android 5.0 (API 21) müssen **dynamisch gelinkte** Programme
position independent (PIE) sein. Voll statische Programme sind davon
ausgenommen, weil der Android-Linker sie gar nicht anfasst - deshalb
funktionieren beide Varianten. PIE ist der Standard, weil es die übliche
Bauform für Android-Engines ist (libc/libm/libdl sind auf jedem Gerät
vorhanden).



## Einbau in die eigene App (der normale Weg)

Die Engine ist ein Prozess, der UCI über stdin/stdout spricht. Deine App ist
der Host - ein Fremd-GUI wird **nicht** gebraucht. Vollständiges Beispiel
(Kotlin, C#) in **`docs/APP_INTEGRATION.md`**.

Kurzfassung:

1. `build\android-<abi>\virgin` als Asset mitliefern, z. B.
   `assets/virgin-arm64-v8a` (bzw. `-armeabi-v7a`, `-x86_64`).
2. Beim ersten Start nach `filesDir/engine/virgin` kopieren und
   `setExecutable(true, false)` setzen (`chmod 755`).
3. Die Dateien aus `data\` nach `filesDir/engine/data/` kopieren - dort und in
   `./data/` sucht die Engine ihre Datendateien.
4. Prozess mit Arbeitsverzeichnis `filesDir/engine` starten, dann
   `uci` -> `uciok`, `isready` -> `readyok`,
   `setoption name Bot value <id>`, `position ...`, `go ...`,
   Zeilen lesen bis `bestmove`.

**Ab Android 10 (API 29)** darf aus dem SD-Speicher nicht ausgeführt werden
(W^X). Deshalb der Kopierschritt in den internen Speicher - genau das machen
andere GUIs auch.

## Optional: Fremd-GUI als Gegenprobe

Nur sinnvoll zur Fehlersuche (liegt es an der App oder an der Engine?):

* **DroidFish:** Binary nach `/sdcard/DroidFish/uci/virgin` legen oder als Zip
  über "Install Engine" einspielen; DroidFish kopiert selbst in den internen
  Speicher und setzt das Ausführungsrecht.
* **Chess for Android:** *UCI and XBoard* -> "Install Engine from SD" ->
  "Import Engine".

Für den normalen Ablauf ist das nicht nötig: `virgin --selftest` prüft die
Engine (43 Prüfungen), `virgin --selfplay v1200-furchtsam v1200-stier 60 5`
spielt eine Partie zwischen zwei Bot-Profilen.

## Auf dem Gerät prüfen

```text
# In einem Terminal (Termux) oder über die GUI
virgin --perft 5      # muss 4865609 ausgeben
virgin --help
virgin --list-bots    # 215 Bots
```

Innerhalb einer GUI: `virgin` starten, dann in der Engine-Konsole
`compiler` (zeigt `target android`) und `bench 8` (Geschwindigkeit)
aufrufen. Wenn `bench` deutlich unter dem Windows-Wert liegt, ist das
normal - ein Handy-Kern ist langsamer, und es läuft nur ein Thread.

## Nicht möglich

Die Android-Binary lässt sich auf Windows **nicht** testen (sie braucht
Androids libc). Ein Emulator wäre möglich (`-Abi x86_64` plus AVD), aber für
die Abnahme zählt das echte Gerät: Binary importieren, eine Partie spielen.
