# Virgin 0.8.5 – Linux UCI

Statisch mit musl/C++-Laufzeit gelinkt, ohne nachzuinstallierende Engine-Bibliotheken.
Ziel: Linux-Kernel 5.10 oder neuer. Ausführungsrecht bei Bedarf: `chmod +x virgin-0.8.5-linux-*`.

- x86_64: x64-Baseline, keine AVX-/BMI2-Pflicht.
- armv6: ARM1176JZF-S, Hardfloat, VFPv2; z. B. ursprünglicher Raspberry Pi / Pi Zero.
- armv7: ARMv7-A, Hardfloat, VFPv3-D16; keine NEON-Pflicht.

`data/` neben den Engines belassen. UCI über stdin/stdout; Abfrage `performance` wie
auf Android/Windows. Das sind Linux-Dateien, keine Android-Binärdateien.
Linux x64 wurde unter WSL ausgeführt. ARMv6/v7 wurden gebaut und statisch geprüft,
aber nicht auf ARM-Hardware ausgeführt. Compiler: Zig 0.15.2; Details in targets.json.
