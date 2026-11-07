# Einbindung in die eigene App

Die Engine ist **kein Programm mit Oberfläche**. Sie ist ein Prozess, der UCI
über stdin/stdout spricht. Deine App ist die GUI: sie startet den Prozess,
sendet Kommandos und liest die Antworten. Ein Fremd-GUI wie DroidFish wird
**nicht** gebraucht.

```text
   Deine App  --(stdin: uci, position, go, setoption)-->  virgin
   Deine App  <--(stdout: info, bestmove, option, uciok)-- virgin
```

**Die Engine braucht nur eine Datei.** Alle 90 Gewichte, die 5 Stilprofile, das
Elo-Profil (700-2800) und der Bot-Katalog stecken in der Binary; `data/` ist
optional (nur für eigene Gewichtstabellen und Anzeigenamen). Nachgewiesen: in
einem Ordner mit ausschließlich `virgin.exe` liefert `uci` alle 103 Optionen,
`setoption name Bot value v1200-stier` setzt 17 Stilgewichte und `go depth 3`
antwortet mit `bestmove`.

## 1. Minimaler Ablauf, den jede App braucht

```text
uci                                   -> "uciok" abwarten
isready                               -> "readyok" abwarten
setoption name Bot value v1200-stier  -> Bot festlegen (Elo + Stil)
position startpos moves e2e4 e7e5     -> Stellung setzen
go wtime 60000 btime 60000 winc 1000 binc 1000
                                      -> "info ..." Zeilen, dann "bestmove g1f3"
stop                                  -> laufende Suche abbrechen (optional)
quit                                  -> Prozess beenden
```

Zugformat: `e2e4`, Umwandlung `e7e8q`. Die Engine prüft die Legalität selbst
und meldet `info string illegal move: <zug>`, wenn die App einen falschen Zug
gesendet hat - gut zum Debuggen der eigenen Zuggenerierung.

## 2. Android (Kotlin)

Die Binary muss im **internen** Speicher der App liegen (ab Android 10 ist das
Ausführen aus dem SD-Speicher gesperrt). Üblicher Weg: Binary als Asset
mitliefern und beim ersten Start kopieren.

```kotlin
private fun abiAsset(): String =
    when {
        Build.SUPPORTED_ABIS.any { it == "arm64-v8a" }   -> "virgin-arm64-v8a"
        Build.SUPPORTED_ABIS.any { it == "armeabi-v7a" } -> "virgin-armeabi-v7a"
        else                                             -> "virgin-x86_64"
    }

/** Kopiert Binary + Daten in den internen Speicher und macht sie ausführbar. */
private fun installEngine(context: Context): File {
    val dir = File(context.filesDir, "engine").apply { mkdirs() }
    val exe = File(dir, "virgin")

    if (!exe.exists() || exe.length() == 0L) {
        context.assets.open(abiAsset()).use { input ->
            exe.outputStream().use { input.copyTo(it) }
        }
    }
    exe.setExecutable(true, false)                 // chmod 755, Pflicht!

    // Datendateien nach <engine>/data/ (die Engine sucht sie dort und in ./data/)
    val data = File(dir, "data").apply { mkdirs() }
    for (name in listOf("weights_default.txt", "styles.txt",
                        "elo_profile.txt", "bots.txt", "bots.json")) {
        val target = File(data, name)
        if (!target.exists()) {
            context.assets.open("data/$name").use { input ->
                target.outputStream().use { input.copyTo(it) }
            }
        }
    }
    return dir
}

class EngineHost(context: Context) {
    private val engineDir = installEngine(context)
    private val process = ProcessBuilder(File(engineDir, "virgin").absolutePath)
        .directory(engineDir)          // Arbeitsverzeichnis = Engine-Ordner
        .redirectErrorStream(true)
        .start()

    private val writer = process.outputStream.bufferedWriter()
    private val reader = process.inputStream.bufferedReader()
    private val lock = Any()

    fun send(line: String) = synchronized(lock) {
        writer.write(line); writer.newLine(); writer.flush()
    }

    /** Liest Zeilen bis einschliesslich der Zeile, die mit [until] beginnt. */
    fun readUntil(until: String, onInfo: (String) -> Unit = {}): String {
        while (true) {
            val line = reader.readLine() ?: error("Engine beendet")
            when {
                line.startsWith(until) -> return line
                line.startsWith("info") -> onInfo(line)
            }
        }
    }

    fun start() {
        send("uci");     readUntil("uciok")
        send("isready"); readUntil("readyok")
    }

    fun newGame(botId: String) {
        send("ucinewgame")
        send("setoption name Bot value $botId")
        send("isready"); readUntil("readyok")
    }

    /** Liefert den besten Zug; [onInfo] bekommt Tiefe/Bewertung fuer die Anzeige. */
    fun bestMove(moves: List<String>, wtime: Long, btime: Long,
                 winc: Long = 0, binc: Long = 0,
                 onInfo: (String) -> Unit = {}): String {
        val pos = if (moves.isEmpty()) "position startpos"
                  else "position startpos moves " + moves.joinToString(" ")
        send(pos)
        send("go wtime $wtime btime $btime winc $winc binc $binc")
        return readUntil("bestmove", onInfo).split(" ")[1]
    }

    fun stop() { send("stop") }
    fun quit() { send("quit"); process.waitFor() }
}
```

Wichtig:
* `setExecutable(true, false)` ist Pflicht, sonst startet der Prozess nicht.
* Immer die **PIE-Variante** (`.\build_android.ps1` ohne `-Static`) nehmen -
  das ist die übliche Bauform für Android-Engines.
* Assets können nicht direkt ausgeführt werden, deshalb der Kopierschritt.
* `redirectErrorStream(true)` verhindert, dass die App an einem vollen
  stderr-Puffer hängen bleibt.

## 3. Desktop (C#)

```csharp
var psi = new ProcessStartInfo(enginePath) {
    RedirectStandardInput = true, RedirectStandardOutput = true,
    RedirectStandardError = true, UseShellExecute = false,
    WorkingDirectory = Path.GetDirectoryName(enginePath)!
};
var engine = Process.Start(psi)!;
engine.StandardInput.WriteLine("uci");
```

Das Protokoll ist sprachunabhängig: Engine als Prozess starten, Zeilen auf
stdin schreiben, Zeilen von stdout lesen (bis `uciok` / `readyok` /
`bestmove`). Für jede andere Sprache gilt genau dasselbe Muster - nur die
Prozess- und Stream-API unterscheidet sich.

Zum Prüfen ohne eigene App: `virgin --selftest` (43 Prüfungen) und
`virgin --selfplay <botA> <botB> <moves> <depth>` (spielt eine Partie zwischen
zwei Bot-Profilen und schreibt ein Log nach `reports/`).

## 4. Bot-Katalog in der App

* `data/bots.json` mitliefern: Stile (mit allen Gewichten), Elo-Profil und die
  215 Bots (id, elo, style, Anzeigename, Tags).
* Bot starten: `setoption name Bot value <id>`.
* Regler "Mut" 1..5: `setoption name Boldness value <1..5>`.
* Feintuning: einzelne Gewichte per `setoption name <Gewicht> value <Zahl>`,
  z. B. `setoption name Aggression.KingAttack value 260`.
* Eigene Bots speichern: Gewichte als Textdatei (`Name = Wert`) schreiben, dann
  `setoption name WeightsFile value <pfad>` + `setoption name ReloadWeights`.
* Optionen abfragen: `listbots` (Katalog), `weights` (alle 90 Werte).

## 5. Debug-Hilfen während der Integration

| Kommando | Zweck |
|---|---|
| `compiler` | zeigt Compiler und Zielplattform (z. B. `target android`) |
| `bench 8` | Geschwindigkeit prüfen (NPS) |
| `sanity` | prüft Board, Bitboards und Zobrist-Schlüssel |
| `perft 5` | muss `4865609` liefern - Movegenerator korrekt |
| `d` | Brett als Text |
| `eval` | Bewertung mit Aufschlüsselung (Material, PST, Tempo) |
| `listbots` | Katalog ausgeben |
| `--perft 5`, `--list-bots`, `--help` | direkt beim Prozessstart, ohne UCI |

## 6. Wozu Fremd-GUIs dann überhaupt gut sind

Nur als **Gegenprobe**: Wenn die Engine in deiner App nicht startet, in
DroidFish aber schon, liegt der Fehler in der App (Kopieren, `chmod`,
Arbeitsverzeichnis, Puffer). Umgekehrt genauso. Als Pflichtschritt ist das
nicht nötig - die Engine-Modi `--selftest` und `--selfplay` erfüllen denselben
Zweck.
```
