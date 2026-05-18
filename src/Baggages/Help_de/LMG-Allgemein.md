### Allgemein

(c) OpenKNX, Steffen Rittmeier 2026

Die vollständige Projektdokumentation ist unter https://github.com/OpenKNX/OFM-LightManager verfügbar.

Das Modul übernimmt die Berechnung der HCL-Sollwerte:

`Stützpunkte / Astro / Sensor` → `OFM-LightManager` → `Status-KOs / Konsumenten-Modul`

**Wichtig:**
- Das Modul stellt nur Sollwerte bereit. Die eigentliche Ausgabe an Leuchten erfolgt in den jeweiligen Ziel-Modulen oder GA´s.
- Parameter und KOs müssen in ETS konsistent projektiert werden.
- Logikfunktionen (Saison-Logik, Zentralfunktionen) können in dedizierten Logikmodulen umgesetzt werden.
