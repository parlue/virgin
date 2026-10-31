<p align="center">
  <img src="assets/virgin-logo.png" alt="Virgin" width="360">
</p>

# Virgin

**A UCI chess engine built for bots.**

Virgin is an aggressive, independent tactician; its strategic reliability still varies. That is the character it ships with, and it is the reason the engine exists: every part of its evaluation can be steered from the outside, so you can shape that character into many different opponents.

* **Make your own bots.** A bot is a playing strength combined with a playing style. Virgin ships a catalogue of **215 ready-made bots** (43 strength levels x 5 styles), and you can define as many more as you like with a plain text file.
* **Every weight is adjustable.** About 90 evaluation weights are exposed as UCI options and in one weights file. No NNUE, no opaque network: you can read, change and reload each value.
* **One small file.** C++20, no external libraries. The Windows binary is under 500 KB, the static Android binary under 1 MB.
* **Windows and Android** from the same code base (Android: arm64-v8a, armeabi-v7a, x86_64).
* **Plain UCI.** Works with any UCI host: BearChess, Arena, Cute Chess, your own app.

## The five styles

The style axis is called *Boldness*, from cautious to reckless:

| Level | Id | Meaning |
|---|---|---|
| 1 | `furchtsam` | timid: safety first, avoids sharp lines, accepts draws |
| 2 | `vorsichtig` | cautious: solid and positional |
| 3 | `ausgeglichen` | balanced: the reference style, all weights at default |
| 4 | `angriffslustig` | aggressive: goes for the king, avoids queen trades |
| 5 | `stier` | bull: sacrifices, pawn storms, king hunts |

Style changes the evaluation weights (king attack, pawn storm, initiative, sacrifice tolerance, contempt, and more) and adds controlled randomness in the root move choice, so the bots feel different from each other, not just weaker.

## Create a bot

### 1. Pick one from the catalogue

```text
setoption name Bot value v1200-stier
```

Bot ids have the form `v<elo>-<style>`, for example `v700-furchtsam`, `v1500-angriffslustig` or `v2800-ausgeglichen`. The full catalogue is in `data/bots.txt`. `virgin --list-bots` prints it, and `virgin --gen-data` also writes it as `data/bots.json` for apps.

### 2. Combine strength and style yourself

```text
setoption name UCI_LimitStrength value true
setoption name UCI_Elo value 1250
setoption name Style value Angriffslustig
setoption name Boldness value 45
```

`Boldness` runs from 10 (timid) to 50 (bull) and interpolates between the styles. `UCI_Elo` accepts any whole number, not only the 50-point steps of the catalogue.

### 3. Tune single weights

```text
setoption name Aggression.KingAttack value 260
setoption name Misc.MoveNoise value 40
```

### 4. Save your bot as a weights file

A bot is just a text file with `Name = value` lines:

```text
# my-bot.txt
Aggression.KingAttack = 260
Aggression.PawnStorm = 180
Aggression.AvoidQueenTrade = 220
Misc.Contempt = 25
```

Load it with:

```text
setoption name WeightsFile value C:\bots\my-bot.txt
setoption name ReloadWeights
```

 `data/weights_default.txt` lists every weight with its default and range.

## UCI options

| Option | Meaning |
|---|---|
| `Bot` | select a catalogue bot, e.g. `v1200-stier` (sets strength and style at once) |
| `UCI_LimitStrength`, `UCI_Elo` | limit playing strength (700 to 3000) |
| `Style`, `Boldness` | playing style and its fine control |
| `WeightsFile`, `ReloadWeights` | load your own weights file |
| `Misc.Contempt`, `Misc.MoveNoise` | draw attitude and root move randomness |
| `Hash`, `Threads`, `Ponder` | the usual: hash size in MB, Lazy SMP threads, pondering |
| `<Group>.<Term>` | every evaluation weight, e.g. `Aggression.KingAttack` |

Extra commands besides the standard ones: `d`, `eval` (evaluation breakdown), `perft <n>`, `bench <n>`, `sanity`, `weights`, `listbots`.

## Build

You need CMake 3.20 or newer and a C++20 compiler.

```powershell
.\build_win.ps1                  # Windows x64 (MSVC)
.\build_android.ps1              # Android arm64-v8a, needs the NDK
.\build_android.ps1 -Static      # fully static Android binary
```

Then run the built-in checks:

```powershell
.\build\win\virgin.exe --selftest       # built-in test run
.\build\win\virgin.exe --perft 5        # must print 4865609
.\build\win\virgin.exe --gen-data       # regenerate data\ (weights, styles, bot catalogue)
```


## Use it in your own app

Virgin is a plain process that speaks UCI over stdin and stdout. Your app is the host: start the process, send commands, read the replies.

## License

MIT, see LICENSE. Credits for the data and ideas Virgin builds on are in THIRD_PARTY.txt.
