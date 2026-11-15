# OEX APK Builder (Windows)

Builds signed Android Open Exchange chess-engine packages from existing Android UCI executables. Configuration controls package ID, name, author, version, ABI paths, SDK versions, notices, output and signing directory. It does not convert arbitrary applications or Windows executables into Android apps.

Requirements: Python 3.9+, Android SDK Build Tools 36.0.0 / Platform 36, and a JDK (Android Studio's bundled `jbr` works). No Gradle or Python dependencies are needed. SDK defaults to `%LOCALAPPDATA%\Android\Sdk`; environment overrides are `ANDROID_HOME` / `ANDROID_SDK_ROOT` and `JAVA_HOME`.

1. Copy `engine.example.json` to `engine.json`.
2. Set a unique lowercase package ID, engine name, author, version and positive version code.
3. Point `binaries` to Android PIE executables; remove unsupported ABIs. Paths are relative to the configuration file. Supported ABIs: arm64-v8a, armeabi-v7a, x86_64, x86.
4. Supply the actual engine/dependency license and notice files in `licenses`. Observe any source distribution obligations separately.
5. Run:

```bat
build.cmd engine.json
```

Or `python build.py engine.json`. Options: `--sdk PATH`, `--jdk PATH`, `--output PATH`, `--validate-only`. Build Tools and target SDK can be changed in the JSON when those SDK components are installed. Minimum SDK must be at least 21.

The APK and a SHA-256 JSON report appear in `dist` by default. Install the APK and select the engine in an OEX-compatible host. The launcher provides installation information, not a chess GUI. Each build keeps its intermediate files in a separate `oex-build-*` directory; remove those manually when no longer needed.

**Keep `signing_directory` private and backed up.** It contains the generated `release.jks` and `password.txt`. Updates require the same package ID and signing key, and an increased `version_code`. Never distribute these secrets. The supplied ignore file covers default private/build outputs.

Executables must be Android PIE programs, not JNI shared libraries. The builder validates ELF architecture, Android interpreter and 16 KB LOAD alignment. Compile with the Android NDK and `-Wl,-z,max-page-size=16384`; static C++ linkage is recommended. See [German instructions](README_DE.md) for a CMake example. This package supports self-contained engines; additional network/data files and private shared libraries require further integration. ELF checks do not replace actual UCI tests or verify every CPU feature/dependency.

The generated package implements `intent.chess.provider.ENGINE`, authority metadata, `res/xml/enginelist.xml`, and a read-only binary ContentProvider. Executables are stored uncompressed in assets and native-library ABI directories. The host must support OEX and execute the engine. UCI options are preserved unchanged.

Protocol references: [DroidFish resolver](https://github.com/peterosterlund2/droidfish/blob/master/DroidFishApp/src/main/java/com/kalab/chess/enginesupport/ChessEngineResolver.java), [ChessEngines provider](https://github.com/c4akarl/ChessEngines/blob/master/ChessEngineSupportLibrary/chessEngineSupportLibrary/src/main/java/com/kalab/chess/enginesupport/ChessEngineProvider.java).

`virgin.example.json` expects the builder at `virgin/tools/oex-apk-builder/`, with the four already compiled binaries at `virgin/build/android-oex-ABI/virgin`. It uses a separate test package/key and does not update the previously delivered Virgin APK. Adjust paths as needed and add the NDK NOTICE files for your toolchain to `licenses`. The supplied Virgin license applies to Virgin only, not other engines.
