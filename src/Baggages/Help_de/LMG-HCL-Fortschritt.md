### HCL-Fortschritt (F4)

Per-Kanal aktivierbar über `ProgressEnable`. Sendet:

- **K14 Tagesfortschritt** (DPT 5.001, 0…100 %): lineare Position zwischen erstem und letztem aktiven Stützpunkt des Tages.
- **K15 Tagesphase** (DPT 5.010): 0 = vor Sonnenaufgang, 1 = Vormittag, 2 = Mittag (±1 h um Solar-Noon), 3 = Nachmittag, 4 = Abend, 5 = Nacht.

Quelle für die Phasen-Berechnung ist die interne Sonnenstands-Engine.
