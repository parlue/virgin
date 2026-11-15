# Virgin 0.8.4 — Open Exchange APK

Author: Dirk D. Sommerfeld. Engine: MIT, current general 0.8.4 sources.

Install `Virgin-0.8.4-OEX.apk`, then select **Virgin 0.8.4** in an OEX-compatible chess GUI. The launcher screen is only installation help; play, analysis and UCI settings belong to the chess GUI. Default is the normal full-strength engine. Enable `Adaptive` in the GUI for adaptive play. Styles: Fortress, Stone, Normal, Fire, Thunder. Elo: `UCI_LimitStrength` and `UCI_Elo`.

No NNUE/network file, external data or Internet permission is required. Minimum Android 5.0/API 21. Universal package: arm64-v8a, armeabi-v7a, x86_64, x86. ARM builds use baseline NDK targets, not optional ARMv8.2 instructions.

OEX structure: exported activity action `intent.chess.provider.ENGINE`, metadata `chess.provider.engine.authority`, resource `res/xml/enginelist.xml`, read-only content provider, and one ABI-specific engine file per list entry. Uncompressed assets support classic resolver copying; extracted native-library files also support hosts executing an installed binary directly. ELF and APK native files are aligned for 16 KB pages.

Build from the engine repository root:

    python android/oex/build.py

Requires Python 3, CMake, Ninja, Android SDK build-tools 36/platform 36, NDK 27, and Java JDK (Android Studio's bundled JBR works). Override paths with ANDROID_HOME, JAVA_HOME, or arguments `--sdk`, `--ndk`, `--jdk`. `--skip-native` reuses already built binaries.

The first build creates a dedicated release signing key and its password under `private/`. Keep these private files locally to sign compatible updates; they are excluded from the source distribution. No debug key is used for the final APK.

Protocol references:
- https://github.com/peterosterlund2/droidfish/tree/master/DroidFishApp/src/main/java/com/kalab/chess/enginesupport
- https://github.com/c4akarl/ChessEngines

The provider implementation in this project is original; its interface is compatible with these references. The engine source and build scripts are included in the release source archive.
