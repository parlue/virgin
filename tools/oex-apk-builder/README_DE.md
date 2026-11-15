# OEX APK Builder für Windows

Verpackt eine Android-UCI-Schachengine als installierbare, signierte Open-Exchange-APK. Name, Autor, Version und Engine-Dateien stehen in einer JSON-Datei. Unterstützt arm64-v8a, armeabi-v7a, x86_64 und x86; nicht benötigte Architekturen können entfallen.

Dies ist kein Konverter für beliebige Apps oder Windows-EXE-Dateien. Die Engine muss bereits als Android-Kommandozeilenprogramm vorliegen. Der Builder ändert weder Spielstärke noch UCI-Optionen. Engines mit zusätzlichen Netz-/Datendateien oder privaten Shared Libraries benötigen eine entsprechende Erweiterung des Pakets und ihrer Dateisuche.

## Voraussetzungen

- Windows, Python 3.9 oder neuer; keine zusätzlichen Python-Pakete.
- Android SDK: Build Tools 36.0.0 und SDK Platform 36.
- JDK, beispielsweise das mit Android Studio installierte `jbr`.
- Android-PIE-Engine-Dateien und deren Lizenz-/Abhängigkeitshinweise.

Standardpfade: SDK unter `%LOCALAPPDATA%\Android\Sdk`, JDK unter `C:\Program Files\Android\Android Studio\jbr`. `ANDROID_HOME`, `ANDROID_SDK_ROOT` bzw. `JAVA_HOME` werden berücksichtigt. Abweichende Pfade lassen sich angeben:

```bat
build.cmd engine.json --sdk "D:\Android\Sdk" --jdk "D:\Java\jdk-21"
```

## Anwendung

1. `engine.example.json` als `engine.json` kopieren.
2. `package` auf eine eigene, eindeutige Paketkennung setzen. Name, Autor und Version eintragen. `version_code` bei jedem veröffentlichten Update erhöhen; `package` beibehalten.
3. In `binaries` die Pfade zu den Android-Executables eintragen. Die Pfade beziehen sich auf den Ordner der JSON-Datei. Nicht vorhandene ABIs entfernen. Die Programme dürfen beliebige Dateinamen haben; intern erzeugt der Builder passende `libengine-ABI.so`-Namen.
4. In `licenses` die tatsächlichen Lizenz- und Notice-Dateien der Engine und ihrer Abhängigkeiten angeben. Bei Copyleft-Lizenzen gegebenenfalls auch den entsprechenden Quellcode bereitstellen; der Builder erstellt kein Quellcodeangebot.
5. Im Terminal bauen:

```bat
build.cmd engine.json
```

Alternativ: `python build.py engine.json`. Nur die Eingaben prüfen: `build.cmd engine.json --validate-only`. Ein anderer Ausgabeordner ist mit `--output "D:\Ausgabe"` möglich.

Die fertige APK und eine JSON-Datei mit SHA-256-Prüfsummen liegen im konfigurierten `output`-Ordner, standardmäßig `dist`. APK auf Android installieren und die Engine in einer OEX-kompatiblen Schach-App auswählen. Die installierte APK selbst enthält keine Schachoberfläche.

## Schlüssel und Updates

Beim ersten Build entsteht unter `signing_directory` ein eigener Signaturschlüssel mit zufälligem Passwort. **Diesen privaten Ordner sichern und niemals veröffentlichen.** Er enthält `release.jks` und `password.txt`. Für Updates müssen Paketkennung und Schlüssel gleich bleiben; `version_code` muss steigen. Ein anderer Schlüssel kann eine vorhandene Installation nicht aktualisieren. Die mitgelieferte `.gitignore` berücksichtigt den standardmäßigen privaten Ordner.

Der Builder arbeitet ohne Gradle und ohne Downloads. Er erzeugt für jeden Lauf einen eigenen `oex-build-*`-Ordner unter dem Ausgabeordner. Diese Zwischenstände können nach erfolgreichem Build manuell entfernt werden. Signatur und ZIP-Ausrichtung werden automatisch geprüft.

## Android-Engine selbst kompilieren

Das ist vom Buildsystem der jeweiligen Engine abhängig. Für ein CMake-Projekt mit ausführbarem Ziel lautet ein typisches Muster:

```bat
cmake -S engine-source -B build-arm64 -G Ninja ^
 -DCMAKE_TOOLCHAIN_FILE="C:/Android/Sdk/ndk/27.0.12077973/build/cmake/android.toolchain.cmake" ^
 -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 ^
 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release ^
 -DCMAKE_EXE_LINKER_FLAGS="-Wl,-z,max-page-size=16384"
cmake --build build-arm64 --parallel 2
```

SDK-/NDK-Pfad anpassen; für jede gewünschte ABI wiederholen. Ein JNI-Shared-Library-Ziel genügt nicht: benötigt wird ein ausführbares UCI-Programm mit Android-Linker. Der Builder prüft ELF-Architektur, PIE-Typ, Android-Interpreter und mindestens 16-KB-LOAD-Ausrichtung. Das ersetzt keinen UCI-Laufzeittest auf den unterstützten Geräten. CPU-Sonderfunktionen und dynamische Abhängigkeiten müssen zum Zielgerät passen.

## OEX-Struktur

Die APK meldet `intent.chess.provider.ENGINE`, die Provider-Autorität und `res/xml/enginelist.xml`. Ein schreibgeschützter ContentProvider liefert die unveränderten Executables. Sie liegen unkomprimiert zusätzlich unter `lib/ABI/`, damit moderne Hosts installierte native Executables verwenden können. Das Paket ist kein allgemeiner UCI-Hintergrunddienst; die Host-App muss OEX unterstützen und die Engine starten.

Für Virgin das normale Android-Executable der allgemeinen Engine verwenden, nicht die Core2-Firmware. Einstellungen wie Adaptive bleiben der Host-App über UCI zugänglich.

## Virgin-Beispiel

`virgin.example.json` erwartet diesen Builder unter `virgin/tools/oex-apk-builder/` und die bereits kompilierten vier Executables unter `virgin/build/android-oex-ABI/virgin`. Mit `build.cmd virgin.example.json` entsteht ein separat signiertes Testpaket. Es ersetzt die bestehende Virgin-OEX-Installation nicht. F?r andere Ablageorte die Pfade anpassen. Erforderliche NDK-Notices zus?tzlich in `licenses` aufnehmen, z. B. `NOTICE` und `NOTICE.toolchain` aus dem verwendeten NDK. Die Virgin-Lizenz gilt nur f?r Virgin; sie ist kein Ersatz f?r die Lizenz anderer Engines.
