# Virgin 0.8.7 – M5Stack Core2

Autor: Dirk D. Sommerfeld.

Der Core2 startet mit dem Schachbrett, ohne BLE-Suche oder automatische Verbindung.
Tippe **Verbinden**, warte auf die Suchliste und tippe dein Brett an.
Nach einer Trennung erfolgt keine automatische Wiederverbindung.
Eine vollstaendige Grundstellung startet automatisch eine neue Partie mit dir als Weiss.
Das gilt auch waehrend einer Suche, beim Nachziehen und nach Partieende;
beide physischen Brettausrichtungen werden erkannt. Wiederholte identische
Grundstellungsmeldungen starten die Partie nicht erneut.
Mit **Move** uebernimmt Virgin die Seite am Zug, du wechselst zur Gegenseite;
die Bildschirmansicht dreht sich mit. So kann Virgin direkt Weiss spielen.
Waehrend einer Suche beendet Move die Suche vorzeitig. Ein noch nicht
nachgezogener Engine-Zug muss zuerst am Brett ausgefuehrt werden.
Virgin startet im adaptiven Modus. Touch-Taste B (Mitte unter dem Display) schaltet
zwischen Adaptive und voller Staerke ohne Elo-Begrenzung um; das Suchbudget bleibt 2,5 Sekunden.
Der gewaehlte Modus bleibt bei einer neuen Partie erhalten.
Touch-Taste A (links) setzt die Partie auf Grundstellung. Die BLE-Verbindung bleibt bestehen.
Am virtuellen Gateway wird die vollstaendige Grundstellung an den PhoeniX weitergereicht;
kein zusaetzlicher Reset-Befehl wird an den Schachcomputer gesendet. Bei einem echten
Brett werden stattdessen die Figuren wieder in Grundstellung aufgebaut.
Die Kopfzeile zeigt A:Neu und B:Modus sowie ADAPTIV oder VOLLE KRAFT.
Diese Beschriftungen koennen auch auf dem Bildschirm angetippt werden. Der Bildschirm zeigt den Engine-Zug sofort als neue Stellung.
Unter "Nachziehen" fuehrst du ihn am physischen Brett aus. Erst die
vollstaendige Brettstellung bestaetigt den Zug fuer den weiteren Spielablauf.

Rechts erscheinen Suchtiefe, Bewertung aus Sicht von Weiss und die ersten
Zuege der Hauptvariante in Koordinatenschreibweise. Die Werte sind die letzte
abgeschlossene Suchiteration; der adaptive Zug kann von der Hauptvariante abweichen.

Unterstuetzt: BLE-Advertising mit Chessnut (beliebiger Modellzusatz),
Smart Chess sowie Millennium. Protokolle und LED-Zuordnung stammen aus
dem lokalen eChessGateway-Projekt. Verbindung und reale Brettdaten mit Chessnut GO am 2026-11-01 bestaetigt.

Core2: klassischer ESP32, 16 MB Flash, PSRAM erforderlich; kein ESP32-S3-Image.
1 MB Hash in PSRAM, 72 KiB interner Suchstack, maximal 20 Halbzuege im Suchpfad,
Suchbudget 2,5 Sekunden. Keine Netz-/NNUE-Dateien. MCU-Spielstaerke nicht gemessen.

Installation mit esptool (COM-Port gegebenenfalls anpassen):

```
python -m esptool --chip esp32 --port COM17 --baud 460800 write_flash --flash_mode dio --flash_freq 40m --flash_size 16MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Source im Repository: ../../esp32_core2 plus ../../src.
Build: platformio run -d esp32_core2 (espressif32 6.8.1 / ESP-IDF 5.3).
Read-only UART commands: status, screenshot at 115200 baud.
Dieser Entwicklungsbuild enthaelt zusaetzlich lokale UART-Testhilfen
tap X Y und testboard mit 64 Figurenzeichen. Fuer normalen Touch-/BLE-Betrieb
werden sie nicht verwendet. Ein Neustart setzt das virtuelle Testbrett zurueck.

Figuren: Merida, Armando Hernandez Marroquin, GPLv2 oder spaeter;
lokale eChessTactics-WebP-Vorlagen auf feste Pixelkonturen reduziert.
Vorlagen und Generatorsource liegen in sources. GPLv3-Kopie liegt bei.
Bibliotheken: M5Unified, M5GFX, esp-nimble-cpp; Lizenztexte beiliegend.

Chessnut-Korrektur 1 (2026-11-01): explizite GATT-Diensteerkennung nach Verbindungsaufbau, optionale Misc-Notifications analog Gateway, serielle Scan-/Verbindungsdiagnose. Erfolgreich gebaut. Auf Core2 installiert; Chessnut GO verbunden, Brettdaten empfangen und 1.e4 d5 physisch erkannt (moves=2).

Anzeige-Korrektur 2: 320x240-RGB565-Bildpuffer in PSRAM; fertige Bilder statt sichtbarem Loeschen/Neuzeichnen. Bei reinen Analyseupdates wird nur die Seitenleiste uebertragen. Bildschirmvorschau bleibt beim Anheben und Umsetzen der echten Figuren bestehen. 24 Modell-/Protokollpruefungen bestanden; Engine-Vorschau auf Core2 per Displayaufnahme und UART-Zustand bestaetigt.

Logo-Dame: sechs Originalzustaende aus BeatVirgin, ohne Balken. Die Stimmung gilt aus Spielersicht nach Virgins Zug, bevorzugt aus dem Adaptive-Protokoll (after), sonst aus der negierten Suchbewertung. Remis und neue Partie neutral; Matt entsprechend Gewinner/Verlierer. Seitenwechsel dreht die Bewertung um. Bilder als 42x62-RGB565-Flashdaten; Original-WebPs und smiley-Schwellen unter assets/queen_moods. Suchwerte bleiben aus Sicht von Weiss.

Certabo und Tabutronic Spectrum via Certabo BLE/UART-Bridge (NUS): manuell
unter Verbinden auswaehlen. Beide koennen als Certabo erscheinen. Bei jeder
Verbindung werden die 34 Figuren-IDs frisch gelernt, wie im eGateway:
Grundstellung mit allen 32 Figuren plus WEISSER Zusatzdame d3 und SCHWARZER
Zusatzdame d6 fuer Certabo; fuer Spectrum Zusatzdamen e3 (weiss) und e6 (schwarz).
Nach stabiler Erkennung blinken die Mittelfelder dreimal. Zusatzdamen entfernen;
erst die vollstaendige normale Grundstellung gibt die Partie frei.
Keine Typauswahl aus L/D-Markern, Paketgroesse, MAC oder Figuren-ID-Praefix.
Vor der Damen-Geste werden keine LED-Kommandos gesendet. Unbekannte/doppelte
Figuren-IDs werden nicht als leere Felder ausgegeben. Verlorene BLE-Fragmente
verwerfen die teilweise gelesene Stellung und das laufende Lernfenster.
Spectrum-Aufbauhilfe blau bei 80%; waehrend der Partie folgen die Zug- und
Korrektur-LEDs der Spielstandfarbe aus SPIELERSICHT, identisch zu chesstactics:
-400 dunkelrot, -200 orange, 0 hellblau, +200 tuerkis, +400 gruen; dazwischen
stufenlos. Quelle ist dieselbe ausgewaehlte Adaptive-Bewertung wie fuer die Dame.
Normales Certabo behaelt sein einfarbiges 8-Byte-LED-Protokoll; Spectrum 247-Byte
RGB-Eckenprotokoll mit Write Without Response und MTU-gerechten Fragmenten.
Core2-Certabo/Spectrum-BLE und echte LEDs muessen noch am jeweiligen Brett
geprueft werden. USB-Certabo direkt am Core2 ist nicht Teil dieses BLE-Treibers.

Spectrum-Farbkorrektur wie CertaboProtocol.spectrumTint in chesstactics: Bei blau-dominanten Farben werden Rot/Gruen gedrosselt, damit die neutralen LEDs blau statt fast weiss erscheinen. Rot/Orange/Gruen bleiben unveraendert.

Akkuanzeige: waagerechte Batterie und Prozentwert oben rechts, aus M5.Power.getBatteryLevel(). Aktualisierung alle 5 Sekunden; Ladesymbol bei aktivem Laden, rote Fuellung ab 20 Prozent. Unbekannter Messwert als --%. Kopfzeile wird auch bei reinen Sidebar-Updates separat aktualisiert.

Bot-Auswahl: Touch-Taste C (rechts) zeigt alle 17 Elo-Stufen von 900 bis 2500 Elo
in 100er-Schritten als direkt antippbare Kacheln (4 Spalten, 5 Reihen).
Die aktuelle Auswahl ist farbig markiert. Neben 2500 liegt die Zurueck-Taste.
Alle Elo-Bots verwenden Fire (Stufe 4 von 5), nicht Thunder. Die Wahl bleibt
bei einer neuen Partie erhalten. Taste B wechselt wieder zu Adaptive bzw.
voller Staerke. Die Bot-Auswahl erfordert keine neue Brettverbindung.
