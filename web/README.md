# Virgin 0.8.5 â€“ WebAssembly / Browser

Autor: Dirk D. Sommerfeld. EigenstÃ¤ndige Browser-Ausgabe der allgemeinen Virgin-Engine,
mit Adaptive, Elo-Begrenzung, den fÃ¼nf Stilen Fortress/Stone/Normal/Fire/Thunder
und den UCI-Bewertungsoptionen. Keine NNUE-/Netzdateien erforderlich.
Die Android- und Core2-Ausgaben werden durch diesen Build nicht ersetzt.

## Dateien und lokaler Start

- `virgin.wasm`: Engine und C++-Laufzeit als WebAssembly.
- `virgin.js`: passender, von Emscripten erzeugter Lader.
- `engine-worker.js`: UCI-Adapter fÃ¼r einen klassischen Web Worker.
- `index.html`: kleines Integrationsbeispiel, keine vollstÃ¤ndige Schach-App.
- `serve.py`: lokaler HTTP-Server mit den erforderlichen Headern.

Alle Runtime-Dateien zusammen im gleichen Verzeichnis bereitstellen. Falls die
verwendete Emscripten-Version zusÃ¤tzliche `virgin*.worker.js` erzeugt, diese
mit ausliefern. JS und WASM immer aus demselben Build verwenden.

```sh
python serve.py
```

Danach `http://localhost:8080/` im Browser Ã¶ffnen. Nicht per `file://` starten.
FÃ¼r die verÃ¶ffentlichten Engine-Dateien benÃ¶tigt der Nutzer kein Emscripten.

## In die eigene App einbinden

```js
const engine = new Worker('/engine/engine-worker.js');
let started = false;
engine.onmessage = ({data}) => {
  if (typeof data === 'string') {
    console.log(data); // UCI: info, bestmove, readyok, ...
    if (data === 'uciok') engine.postMessage('isready');
    if (data === 'readyok' && !started) {
      started = true;
      engine.postMessage('position startpos moves e2e4');
      engine.postMessage('go movetime 1000');
    }
  } else if (data.type === 'ready') {
    engine.postMessage('uci');
  } else if (data.type === 'error') {
    console.error(data.message);
  }
};
// Bei Bedarf während der Suche:
function stopAnalysis() { engine.postMessage('stop'); }
// Geordnet beenden; alternativ Worker.terminate() beim Verlassen der App:
function closeEngine() { engine.postMessage({type: 'dispose'}); }
```

Jeder Befehl ist genau eine UCI-Zeile ohne Zeilenumbruch. Alternativ wird
`{type:'command', command:'uci'}` angenommen. UCI-Ausgaben sind Strings.
Kontrollmeldungen sind Objekte mit `type: ready`, `error`, `log` oder `closed`.
`ready` bedeutet geladene Runtime; der UCI-Handshake und `readyok` bleiben Aufgabe
der App. Pro Worker gibt es eine Engine-Sitzung. WÃ¤hrend der Initialisierung
werden bis zu 256 Befehle gepuffert. Ausgabe kontinuierlich verarbeiten.

FÃ¼r Adaptive: `setoption name Adaptive value true`. Zum Wechsel auf einen Bot
zuerst Adaptive ausschalten, dann Style, UCI_Elo und UCI_LimitStrength setzen.
Die normalen UCI-Regeln fÃ¼r stop/position/go gelten auch im Browser.

## Hosting und Ressourcen

Diese Ausgabe nutzt WebAssembly-Threads, damit Suche, UCI-Eingabe und stop
parallel arbeiten kÃ¶nnen. Das Dokument und die Engine-Dateien auf derselben
Origin Ã¼ber HTTPS bereitstellen (localhost erlaubt HTTP). Erforderliche Header:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

WASM mit `Content-Type: application/wasm` ausliefern. SharedArrayBuffer und
`crossOriginIsolated` mÃ¼ssen verfÃ¼gbar sein. Bei getrennten Origins sind zudem
passende CORS/CORP-Regeln fÃ¼r sÃ¤mtliche Ressourcen erforderlich. Einbettungen
und externe App-Ressourcen mÃ¼ssen mit der Isolation kompatibel sein. CSP muss
die eigenen Worker und WebAssembly erlauben. Ein einfacher Standardserver ohne
diese Header genÃ¼gt fÃ¼r diesen Thread-Build nicht.

Bis zu vier Suchthreads, automatisch hÃ¶chstens die gemeldete Kernzahl; fÃ¼nf
vorbereitete Laufzeit-Worker fÃ¼r Engine-Steuerung plus Suche. Hash 32 MiB als
Standard, einstellbar bis 256 MiB. WASM-Speicher beginnt bei 128 MiB und kann
bis 512 MiB wachsen; tatsÃ¤chlicher Browser-/Worker-Gesamtverbrauch liegt darÃ¼ber.
Die App kann mit `setoption name Threads value 1` CPU-Verbrauch reduzieren.
Adaptive wÃ¤hlt seine eigenen Einstellungen und Ã¼bersteuert gewÃ¶hnliche Optionen.

## Selbst bauen

Python, CMake, Ninja und das offizielle Emscripten-SDK installieren. Beispielsweise
unter `tools/emsdk` das SDK klonen, installieren und aktivieren:

```sh
git clone https://github.com/emscripten-core/emsdk.git tools/emsdk
python tools/emsdk/emsdk.py install latest
python tools/emsdk/emsdk.py activate latest
python build_web.py
```

FÃ¼r reproduzierbare Builds statt `latest` die Version aus `manifest.json`
verwenden. Ein anderer SDK-Pfad geht mit `--emsdk PFAD`, ein anderer Ausgabeordner
mit `--output PFAD`. Der Windows-Build erkennt die hier Ã¼blichen CMake-/Ninja-Pfade;
auf anderen Systemen mÃ¼ssen CMake und Ninja im PATH stehen.

Quellen: `Virgin-0.8.5-Web-source.zip`. Die Engine behÃ¤lt die MIT-Lizenz in
`ENGINE-LICENSE.txt`; Emscripten-/C++-Laufzeithinweise liegen unter `licenses/`.

Technische Referenzen:
- https://emscripten.org/docs/porting/pthreads.html
- https://emscripten.org/docs/tools_reference/settings_reference.html

## Prüfung dieser Ausgabe

15 gezielte Prüfungen im lokalen Chrome-Browser bestanden, darunter UCI und
Stiloptionen, perft(4) = 197281, zeitbegrenzte Suche mit einem und vier Threads,
Stop einer laufenden Analyse, Adaptive, Elo-Bot, Umwandlung und Beenden während
der Suche. Bericht: `verification.json` im ausgelieferten Paket.

Der Browser-Integrationstest liegt im Quellpaket unter `tools/test_web.cjs` und
benötigt Node 22+, Python und Chrome. Browser/Python können über
`VIRGIN_TEST_CHROME` und `VIRGIN_TEST_PYTHON` angegeben werden. Er verwendet
eine eigene unsichtbare Browser-Sitzung und beendet sie anschließend.

```sh
node tools/test_web.cjs
```
