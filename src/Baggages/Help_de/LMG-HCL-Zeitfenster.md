### HCL-Zeitfenster (F1)

Per-Kanal-Parameter **HCL-Zeitfenster** (`CHHclTimeWindow`):

- **Immer** (Default): kein Zeit-Gate.
- **Nur tagsüber**: K00/K01/K08/K11–K15 senden nur zwischen Sonnenaufgang und Sonnenuntergang.
- **Nur nachts**: nur außerhalb dieses Bereichs.

Achsen- und Zeit-Filter sind **UND**-verknüpft. Tag/Nacht-Quelle ist `DayNightSource`; bei `Aus` greift intern eine Astronomie-Berechnung als Fallback, damit das Zeit-Gate auch ohne projektiertes K06 funktioniert.
