# Zukünftige Aufgabe: M5Chess S3 / Bauerino

Erfasst am 04.11.2026 auf Wunsch von Dirk D. Sommerfeld.
Status: vorgemerkt, noch nicht zur Umsetzung gestartet.

## Zielhardware und Oberfläche

- Bestelltes Entwicklungsboard: ESP32-S3-Touch-AMOLED-2.16.
- Später eine eigene Firmware für den Bauerino bereitstellen. Laut Nutzer ist
  die Displayauflösung identisch; die genaue Boardvariante, Auflösung,
  Display-/Touchcontroller, Pinbelegung und Speicherausstattung vor Umsetzung
  anhand des Produktlinks prüfen.
- Gut bedienbare Touchoberfläche mit klaren Menüs, großen Bedienelementen und
  scharfen Figuren. Anzeige und Touch-/Hardwaretreiber von der Spiellogik trennen.
- Vorhandene M5Chess-Spiel-, Engine- und E-Board-Funktionen weiterverwenden.

## Analyse und Stellungseditor

- Figuren per Touch setzen, ersetzen und entfernen; Brett leeren oder
  Grundstellung laden.
- Zugrecht, Rochaderechte und en-passant-Feld festlegen; ungültige Stellungen
  vor dem Analysestart verständlich melden.
- Aktuelle E-Board-Stellung übernehmen; fehlende Stellungsangaben ergänzen.
- Fortlaufende Virgin-Analyse mit voller Stärke, unabhängig vom Spielmodus
  Adaptive oder Elo-Bot; Bewertung, Tiefe und Hauptvariante anzeigen.
- Varianten auf dem Display ausprobieren, Züge zurücknehmen und zur
  ursprünglichen Stellung zurückkehren.

## Partienspeicher, Viewer und PGN

- Die letzten 20 Partien dauerhaft speichern, auch über das Ausschalten hinaus.
  Bei einer weiteren Partie die älteste ersetzen.
- Partienliste mit Gegner/Bot-Stufe, eigener Farbe, Ergebnis und Datum,
  sofern eine gültige Uhrzeit verfügbar ist.
- Viewer mit Zugliste, direkter Zugauswahl sowie Anfang/Zurück/Vor/Ende.
- Eine Viewer-Stellung zur Analyse öffnen und Varianten ausprobieren, ohne
  die gespeicherte Partie zu verändern.
- Einzelne Partie oder alle gespeicherten Partien als PGN exportieren und
  zum Teilen bereitstellen.
- Als möglicher Übertragungsweg: kleine WLAN-Seite zum Herunterladen am Handy;
  die genaue Lösung beim Umsetzen festlegen.

## Vorhandene Funktionen aus eChessTactics übernehmen

Die entsprechenden Abläufe funktionieren bereits in der eChessTactics-App.
Vor eigener Implementierung deren Code für Stellungsaufbau, Analyse,
Partienspeicherung, Viewer und PGN prüfen und geeignete Logik, Datenformate und
Bedienabläufe übernehmen bzw. auf den ESP32-S3 portieren. Android-spezifische
Darstellung und Speicherung an Display, Touch und Flash des Geräts anpassen.
Die konkreten Quelldateien und wiederverwendbaren Module erst bei Beginn der
Aufgabe identifizieren.

## Umfang dieser Vormerkung

Dies ist eine zukünftige Entwicklungsaufgabe. Aktuell keine neue Firmware
bauen, keine Geräte flashen und den funktionierenden Core2-Stand beibehalten.
