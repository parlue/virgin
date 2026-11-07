# Virgin 0.7.1 als native Android-Bibliothek

Autor: **Dirk D. Sommerfeld**. Gleiche Suche, Bewertung und Adaptive-Auswahl wie Virgin 0.7.1. Keine Netzdateien. Diese Bibliothek läuft direkt im Prozess der App; es wird kein ausführbares Engine-Programm gestartet.

## Fertige Dateien

| Architektur | Datei | Größe |
|---|---|---:|
| arm64-v8a | jniLibs/arm64-v8a/libvirgin.so | 681.008 Byte |
| armeabi-v7a | jniLibs/armeabi-v7a/libvirgin.so | 442.852 Byte |
| x86_64 | jniLibs/x86_64/libvirgin.so | 681.744 Byte |

Android ab API 21. libc++ ist statisch in die Bibliothek eingebunden; keine zusätzliche libc++_shared.so nötig. Es bleiben die Android-Systembibliotheken libc, libm und libdl. Die ELF-LOAD-Segmente sind für 16-KiB-Speicherseiten ausgerichtet. Die Ausrichtung der späteren APK muss zusätzlich vom App-Build korrekt vorgenommen werden.

### Möglichkeit A: AAR einbinden

`virgin-0.7.1.aar` nach `app/libs/` kopieren. In `app/build.gradle.kts`:

```kotlin
dependencies {
    implementation(files("libs/virgin-0.7.1.aar"))
}
```

Die AAR enthält alle drei ABIs, die Java-Klasse und die R8-Regel. Sie enthält keine Schachoberfläche und ist keine installierbare APK. Ein moderner App-Build mit AGP 8.5.1 oder neuer unterstützt das passende Packaging für 16-KiB-Geräte; die endgültige APK danach prüfen.

### Möglichkeit B: Dateien direkt übernehmen

Den Inhalt von `jniLibs/` nach `app/src/main/jniLibs/` kopieren. `java/de/dirksommerfeld/virgin/VirginEngine.java` nach `app/src/main/java/de/dirksommerfeld/virgin/` kopieren. Die mitgelieferte `consumer-rules.pro` in die Proguard-/R8-Regeln der App übernehmen.

Die JNI-Klasse und ihre nativen Methodennamen dürfen nicht umbenannt werden. Beide Möglichkeiten laden die Bibliothek automatisch mit `System.loadLibrary("virgin")`. AAR und direkte Dateien sind Alternativen; nicht doppelt einbinden.

## Verwendung aus Kotlin

UCI bleibt das Protokoll: `send()` nimmt eine Befehlszeile ohne Zeilenumbruch an, `readLine()` liefert eine vollständige Antwortzeile. Die Engine selbst sucht im nativen Hintergrundthread. Das Lesen und Schließen gehört auf einen App-Hintergrundthread. Beispiel für eine einzelne Antwort auf 1.e4:

```kotlin
import de.dirksommerfeld.virgin.VirginEngine
import kotlin.concurrent.thread

thread(name = "Virgin-UCI") {
    VirginEngine().use { engine ->
        fun await(prefix: String): String {
            val deadline = System.nanoTime() + 10_000_000_000L
            while (System.nanoTime() < deadline) {
                val line = engine.readLine(100) ?: continue
                println(line) // info enthält Bewertung, Tiefe und Hauptvariante
                if (line.startsWith(prefix)) return line
            }
            error("Virgin antwortet nicht: $prefix")
        }
        engine.send("uci")
        await("uciok")
        engine.send("setoption name Threads value 1")
        engine.send("isready")
        await("readyok")
        engine.send("ucinewgame")
        engine.send("position startpos moves e2e4")
        engine.send("go movetime 1000")
        val answer = await("bestmove ")
        println("Antwort: $answer") // hier an das Schachbrett der App übergeben
    }
}
```

In einer vollständigen App die Engine während der Partie offenhalten und alle Antworten fortlaufend über einen Leser konsumieren. `info ... score cp 42` bedeutet 0,42 Bauerneinheiten aus Sicht der am Zug befindlichen Seite der analysierten Ausgangsstellung. `bestmove e7e5` ist der ausgegebene Zug. Die App muss die Züge legal auf ihrem Brett anwenden und vor der nächsten Suche die vollständige Stellung senden. Für ein neues Spiel `ucinewgame` und `position startpos` senden.

`setoption name Adaptive value true` schaltet dieselbe Adaptive-Auswahl wie in der bisherigen Engine ein. Dabei gelten die bisherigen Regeln: andere Engine-Einstellungen werden ignoriert; UCI-Suchgrenzen gelten weiterhin. Standard ist Adaptive aus und volle Spielstärke. Die Thread-Vorgabe im Beispiel ist eine App-Entscheidung; ohne diese Zeile bleibt die automatische Threadwahl der Engine aktiv.

## Lebensdauer und Nebenläufigkeit

- Genau **eine offene Engine-Sitzung pro App-Prozess**. Eine zweite Instanz wird abgewiesen, bis die erste geschlossen ist. Die aktuelle Engine verwendet globalen Zustand; mehrere unabhängige Bretter benötigen sequenzielle Sitzungen oder getrennte Prozesse.
- `send`, `readLine` und `close` sind in der Java-Klasse synchronisiert. `readLine` akzeptiert 0 bis 100 ms und liefert bei Timeout oder nach beendetem UCI-Loop `null`. Ein großer Lese-Timeout kann andere Methoden entsprechend verzögern.
- Ein dedizierter Leser muss die Ausgaben kontinuierlich konsumieren; sie werden bis zum Lesen gepuffert. Keine direkten UI-Callbacks aus C++.
- `go infinite` läuft bis `stop`. Vor einer neuen `position` oder `go` während einer laufenden unendlichen Suche zuerst `stop` senden und `bestmove` abwarten.
- `close()` verwirft noch nicht verarbeitete Befehle, fordert den Suchabbruch an, beendet den UCI-Loop und wartet auf die nativen Worker. Danach wird der Hashspeicher freigegeben. Tabellen bleiben für einen späteren Neustart geladen. Nicht auf dem UI-Thread schließen. Den Leser vorher beenden beziehungsweise dessen Lebensdauer mit der Engine koordinieren.
- Nach `quit` ist die Sitzung beendet. Erst `close()`, dann eine neue Instanz erzeugen. Beim Neustart werden Optionen, Bewertung, Adaptive-Zustand, Stellung, Hash und Spielhistorie zurückgesetzt.
- Die Bibliothek lädt beim Start keine losen Gewichtedateien aus dem Arbeitsverzeichnis. Die eingebauten Defaults werden verwendet. Optionales `WeightsFile` kann weiterhin explizit einen zugänglichen App-internen Pfad angeben.

## Native Schnittstelle / andere App-Frameworks

`include/virgin_native.h` exportiert vier C-Funktionen: `virgin_create`, `virgin_send`, `virgin_read`, `virgin_destroy`. Damit ist auch eine direkte FFI-Anbindung möglich. Die JNI-Anbindung ist zusätzlich enthalten. Die C-API dokumentiert Rückgabecodes und Pufferregeln im Header. Die Java-Klasse schützt die Lebensdauer; bei direkter C-/FFI-Nutzung darf `destroy` nicht gleichzeitig mit anderen Aufrufen auf demselben Handle laufen.

## Build und Prüfung

Aus dem Virgin-Projekt: `powershell -ExecutionPolicy Bypass -File build_android_library.ps1 -Abi all`.

Alternativ CMake mit Android-NDK-Toolchain, `-DVIRGIN_LIBRARY=ON`, `-DANDROID_STL=c++_static`, `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` und gewünschter ABI. NDK 27.0.12077973 wurde verwendet. Das Ziel heißt `virgin`, die Android-Ausgabe `libvirgin.so`.

Prüfungen dieser Ausgabe:

- Alle drei Android-Bibliotheken gebaut; ELF-Typ, SONAME, exportierte C-/JNI-Symbole, Systemabhängigkeiten und 16-KiB-Ausrichtung geprüft.
- Java-Klasse kompiliert und für Android in DEX übersetzt.
- JNI-Laufzeittest auf Android 14 / API 34 im x86_64-Emulator: drei Zyklen mit Start, UCI, Suche, Adaptive und Schließen bei laufender Suche bestanden. Kein physisches ARM-Gerät und kein 16-KiB-Android-Laufzeittest vorhanden.
- Host-Bibliothek: Protokoll, kleine Puffer, ungültige Befehle, parallele Instanzabweisung, Suchabbruch und zwölf Neustartzyklen geprüft.
- Bei Tiefe 6 und einem Thread stimmen Bewertung, Knotenzahl und Hauptvariante zwischen Bibliothek und bisheriger ausführbarer Engine exakt überein.
- Alle 116 Engine-Selbsttests nach der Stream-Anbindung bestanden. Keine Gegnerpartien durchgeführt; kein neuer Elo-Nachweis.

Die bisherigen Windows-, Android-Programm- und ESP32-Veröffentlichungen wurden durch diesen zusätzlichen Bibliotheks-Build nicht ersetzt. SHA-256-Prüfsummen der Lieferung stehen in `manifest.json`.

Offizielle Android-Hinweise: [JNI](https://developer.android.com/ndk/guides/jni-tips), [16-KiB-Seiten und APK-Packaging](https://developer.android.com/guide/practices/page-sizes).
