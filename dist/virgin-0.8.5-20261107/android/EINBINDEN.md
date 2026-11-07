# Virgin 0.8.5 – Android-App-Paket

## Einbinden

`virgin-0.8.5.aar` nach `app/libs/` kopieren. In `app/build.gradle.kts`:

```kotlin
dependencies {
    implementation(files("libs/virgin-0.8.5.aar"))
}
```

Mindestens Android API 21. Die AAR enthält Java-Wrapper, JNI-Bibliotheken für
arm64-v8a, armeabi-v7a, x86_64 und x86 sowie die benötigte R8/ProGuard-Regel.
C++-Laufzeit ist statisch eingebunden. Keine zusätzliche libc++_shared.so nötig.
Die AAR genügt: `jniLibs` und Java-Dateien sind nur für manuelles Einbinden
beigelegt; bei Verwendung der AAR nicht zusätzlich kopieren.
Keine APK erforderlich. Die Dateien in `uci/` sind alternativ eigenständige
UCI-Engines für eine bereits vorhandene Prozess-Anbindung.

## Engine ansprechen

```java
import de.dirksommerfeld.virgin.VirginEngine;

VirginEngine engine = new VirginEngine();
engine.send("uci");
// Im vorhandenen Hintergrund-Leser bis uciok lesen.
engine.send("setoption name LivePerformance value true");
engine.send("isready");
// Bis readyok lesen, dann:
engine.send("ucinewgame");
engine.send("position startpos moves e2e4");
engine.send("go movetime 1000");
// Nach bestmove oder während der Suche abfragen:
engine.send("performance");
```

Ein Hintergrund-Leser liest fortlaufend `engine.readLine(20)`; `null` bedeutet
hier Zeitüberschreitung oder beendete Sitzung. Es darf nur einen Leser und eine
Engine-Sitzung pro Prozess geben. UI-Updates auf den Android-Hauptthread schicken.
Beim Beenden `engine.close()` im Hintergrund aufrufen; den Leser vorher stoppen.

Bereits vorhandene UCI-Anbindungen behalten ihren normalen Ablauf bei.
Für jeden neuen Zug die gesamte bestätigte Zugliste mit demselben Startpunkt
senden (`position startpos moves ...` bzw. gleichbleibende Start-FEN plus moves).
Nur die aktuelle FEN ohne Historie reicht für das Drei-Züge-Fenster nicht aus.

## Anzeige abrufen

Senden:

```text
performance
```

Antwort beispielsweise:

```text
info string performance white 2300 black >2800 window 3 active true white_samples 5 black_samples 4 white_ply 9 black_ply 8 pending 0 revision 12 skipped 0 white_lossguard false black_lossguard false status ready
```

Für die Spieleranzeige den Wert hinter `white` oder `black` verwenden, je nach
Spielerfarbe. `>2800` ist Text und darf nicht als Ganzzahl eingelesen werden.
Weitere Felder als Schlüssel/Wert-Paare lesen; unbekannte Felder ignorieren.
`info string performance prepared ...` ist eine andere Antwort und kein Messwert.

- `status initial`: noch keine gemessenen Züge; Startanzeige >2800.
- `status ready`: bestätigte Züge verarbeitet.
- `status pending`: noch Züge offen; angezeigte Werte sind der bisherige Stand.
- `status disabled`: Funktion ausgeschaltet.
- `white_ply`/`black_ply`: zuletzt verarbeiteter Halbzug je Farbe, relativ zum Startpunkt.
- `revision`: Änderungskennung; `skipped`: bei spätem Einstieg übersprungene Halbzüge.

Die Abfrage liefert nur den aktuellen Stand und startet keine Analyse. Die
Berechnung erfolgt vor einer normalen Suche innerhalb eines begrenzten Zeitbudgets.
Bei sehr kurzen Suchzeiten kann sie auf die nächste Suche verschoben werden.
Nach `bestmove` und nach jeder neuen Suche erneut abfragen.
Im Adaptivmodus (`setoption name Adaptive value true`) ist sie automatisch aktiv.
Im normalen Modus schaltet `LivePerformance` sie ein oder aus.

Optional kann die App während der menschlichen Bedenkzeit nach Übergabe der
bestätigten Stellung `performance prepare` senden. Das startet eine kurze
Kandidatenanalyse und beendet sich mit `info string performance prepared depth N`;
es kommt hierfür kein `bestmove`. Vor einer neuen Suche wie üblich `stop` senden.
Für die erste Integration ist dieser Zusatz nicht nötig.

## Bedeutung und Prüfstand

Die Anzeige beschreibt die laufende Partie, keine dauerhafte oder FIDE-Spielstärke.
Basis sind die letzten drei eigenen Züge, anfangs die bereits verfügbaren.
In unverändert verlorener Stellung steigt sie nicht allein durch gute Verteidigungszüge;
eine tatsächliche Stellungsverbesserung kann sie wieder anheben.

Alle acht Android-Binärdateien wurden gebaut und auf Architektur, 16-KiB-ELF-
Segmentausrichtung und Übereinstimmung mit dem aktuellen Build geprüft. Die AAR
enthält exakt die beigefügten vier JNI-Bibliotheken. Die UCI-Integration bestand
14 Prüfgruppen auf Windows. Auf einem Android-Gerät wurde dieses Paket noch
nicht ausgeführt. Quellen zum eigenen NDK-Build liegen in `native-source/`.
